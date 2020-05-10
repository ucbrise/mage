/*
 * Copyright (C) 2020 Sam Kumar <samkumar@cs.berkeley.edu>
 * Copyright (C) 2020 University of California, Berkeley
 * All rights reserved.
 *
 * This file is part of MAGE.
 *
 * MAGE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MAGE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MAGE.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "memprog/replacement.hpp"
#include <cstdint>
#include <array>
#include <string>
#include "memprog/addr.hpp"
#include "memprog/annotation.hpp"
#include "memprog/instruction.hpp"
#include "memprog/opcode.hpp"

namespace mage::memprog {
    Allocator::Allocator(std::string output_file, PhysPageNumber num_page_frames) : num_swapouts(0), num_swapins(0), phys_prog(output_file) {
        this->free_page_frames.reserve(num_page_frames);
        PhysPageNumber curr = num_page_frames;
        do {
            curr--;
            this->free_page_frames.push_back(curr);
        } while (curr != 0);
    }

    Allocator::~Allocator() {
    }

    std::uint64_t Allocator::get_num_swapouts() const {
        return this->num_swapouts;
    }

    std::uint64_t Allocator::get_num_swapins() const {
        return this->num_swapins;
    }

    StoragePageNumber Allocator::get_num_storage_frames() const {
        return this->next_storage_frame;
    }

    StoragePageNumber Allocator::emit_swapout(PhysPageNumber primary) {
        StoragePageNumber secondary;
        if (this->free_storage_frames.empty()) {
            secondary = this->next_storage_frame++;
        } else {
            secondary = this->free_storage_frames.back();
            this->free_storage_frames.pop_back();
        }

        constexpr std::size_t length = PackedPhysInstruction::size(InstructionFormat::Swap);
        PackedPhysInstruction& phys = this->phys_prog.start_instruction(length);
        phys.header.operation = OpCode::SwapOut;
        phys.header.flags = 0;
        phys.header.output = primary;
        phys.swap.storage = secondary;
        this->phys_prog.finish_instruction(length);
        this->num_swapouts++;

        return secondary;
    }

    void Allocator::emit_swapin(StoragePageNumber secondary, PhysPageNumber primary) {
        constexpr std::size_t length = PackedPhysInstruction::size(InstructionFormat::Swap);
        PackedPhysInstruction& phys = this->phys_prog.start_instruction(length);
        phys.header.operation = OpCode::SwapIn;
        phys.header.flags = 0;
        phys.header.output = primary;
        phys.swap.storage = secondary;
        this->phys_prog.finish_instruction(length);
        this->num_swapins++;

        this->free_storage_frames.push_back(secondary);
    }

    BeladyAllocator::BeladyAllocator(std::string output_file, std::string virtual_program_file, std::string annotations_file, PhysPageNumber num_page_frames, PageShift shift)
        : Allocator(output_file, num_page_frames), virt_prog(virtual_program_file.c_str(), false), annotations(annotations_file.c_str(), false), page_shift(shift) {
    }

    void BeladyAllocator::allocate() {
        ProgramFileHeader* header = this->virt_prog.mapping();
        PackedVirtInstruction* current = reinterpret_cast<PackedVirtInstruction*>(header + 1);
        Annotation* ann = this->annotations.mapping();
        std::array<bool, 5> just_swapped_in;
        std::array<PhysPageNumber, 5> ppns;
        std::array<VirtPageNumber, 5> vpns;
        for (InstructionNumber i = 0; i != header->num_instructions; i++) {
            std::uint8_t num_pages = current->store_page_numbers(vpns.data(), this->page_shift);
            assert(num_pages == ann->header.num_pages);
            for (std::uint8_t j = 0; j != num_pages; j++) {
                VirtPageNumber vpn = vpns[j];

                auto iter = this->page_table.find(vpn);
                if (iter != this->page_table.end() && iter->second.resident) {
                    /* Page is already resident; just use its current frame. */
                    just_swapped_in[j] = false;
                    ppns[j] = iter->second.ppn;
                } else {
                    /* Page is not resident. */
                    PhysPageNumber ppn;
                    just_swapped_in[j] = true;

                    if (this->page_frame_available()) {
                        /* Grab page frame from free list. */
                        ppn = this->alloc_page_frame();
                    } else {
                        /* No page frames free; we must swap something out. */
                        /*
                         * We know that this won't evict one of the VPNs we
                         * need for this instruction because all those VPNs
                         * have i as their key, whereas all other VPNs in the
                         * heap will have some later instruction.
                         */
                        std::pair<BeladyScore, VirtPageNumber> pair = this->next_use_heap.remove_min();
                        VirtPageNumber evict_vpn = pair.second;
                        auto k = this->page_table.find(evict_vpn);
                        assert(k != this->page_table.end());
                        PageTableEntry& evict_pte = k->second;
                        assert(evict_pte.resident);
                        ppn = evict_pte.ppn;
                        if (pair.first.get_usage_time() == invalid_instr) {
                            this->page_table.erase(k);
                        } else {
                            evict_pte.resident = false;
                            evict_pte.spn = this->emit_swapout(ppn);
                        }
                    }

                    /* Now, swap the desired vpn into the page frame. */
                    if (iter == this->page_table.end()) {
                        /* First use of this VPN, so no need to swap it in. */
                        assert(j == 0 && (current->header.flags & FlagOutputPageFirstUse) != 0);
                        PageTableEntry& pte = this->page_table[vpn];
                        pte.resident = true;
                        pte.ppn = ppn;
                    } else {
                        /* Swap the desired VPN into the page frame. */
                        PageTableEntry& pte = iter->second;
                        assert(!pte.resident);
                        this->emit_swapin(pte.spn, ppn);
                        pte.resident = true;
                        pte.ppn = ppn;
                    }
                    ppns[j] = ppn;
                }
            }
            for (std::uint8_t j = 0; j != num_pages; j++) {
                InstructionNumber next_use = ann->slots[j].next_use;
                // Optimization: if it's never used again, just mark the page free
                if (just_swapped_in[j]) {
                    this->next_use_heap.insert(next_use, vpns[j]);
                } else {
                    this->next_use_heap.decrease_key(next_use, vpns[j]);
                }
            }

            PackedPhysInstruction& phys = this->phys_prog.start_instruction();
            phys.header.operation = current->header.operation;
            phys.no_args.width = current->no_args.width;
            phys.header.flags = current->header.flags;
            phys.restore_page_numbers(*current, ppns.data(), this->page_shift);
            this->phys_prog.finish_instruction(phys.size());

            current = current->next();
            ann = ann->next();
        }
    }
}
