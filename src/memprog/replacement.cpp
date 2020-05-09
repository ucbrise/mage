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

namespace mage::memprog {
    Allocator::Allocator(std::string output_file) : num_swapouts(0), num_swapins(0), phys_prog(output_file) {
    }

    Allocator::~Allocator() {
    }

    std::uint64_t Allocator::get_num_swapouts() const {
        return this->num_swapouts;
    }

    std::uint64_t Allocator::get_num_swapins() const {
        return this->num_swapins;
    }

    void Allocator::emit_swapout(PhysPageNumber primary, VirtPageNumber secondary) {
        PackedPhysInstruction phys;
        phys.header.operation = OpCode::SwapOut;
        phys.header.width = 1;
        phys.header.flags = 0;
        phys.header.output = primary;
        phys.constant.constant = secondary;
        phys.constant.format = InstructionFormat::Constant;
        this->emit_instruction(phys, PackedPhysInstruction::size(InstructionFormat::Constant));
        this->num_swapouts++;
    }

    void Allocator::emit_swapin(PhysPageNumber primary, VirtPageNumber secondary) {
        PackedPhysInstruction phys;
        phys.header.operation = OpCode::SwapIn;
        phys.header.width = 1;
        phys.header.flags = 0;
        phys.header.output = primary;
        phys.constant.constant = secondary;
        phys.constant.format = InstructionFormat::Constant;
        this->emit_instruction(phys, PackedPhysInstruction::size(InstructionFormat::Constant));
        this->num_swapins++;
    }

    BeladyAllocator::BeladyAllocator(std::string output_file, std::string virtual_program_file, std::string annotations_file, PhysPageNumber num_physical_pages, PageShift shift)
        : Allocator(output_file), virt_prog(virtual_program_file.c_str(), false), annotations(annotations_file.c_str(), false), page_shift(shift) {
        this->free_list.reserve(num_physical_pages);
        PhysPageNumber curr = num_physical_pages;
        do {
            curr--;
            this->free_list.push_back(curr);
        } while (curr != 0);
    }

    void BeladyAllocator::allocate() {
        ProgramFileHeader* header = this->virt_prog.mapping();
        PackedVirtInstruction* current = reinterpret_cast<PackedVirtInstruction*>(header + 1);
        Annotation* ann = this->annotations.mapping();
        std::array<bool, 5> just_swapped_in;
        std::array<PhysPageNumber, 5> ppns;
        std::array<VirtPageNumber, 5> vpns;
        PackedPhysInstruction phys;
        for (InstructionNumber i = 0; i != header->num_instructions; i++) {
            phys.header.operation = current->header.operation;
            phys.header.width = current->header.width;
            phys.header.flags = current->header.flags;
            std::uint8_t num_pages = current->store_page_numbers(vpns.data(), this->page_shift);
            assert(num_pages == ann->header.num_pages);
            for (std::uint8_t j = 0; j != num_pages; j++) {
                VirtPageNumber vpn = vpns[j];

                auto iter = this->page_table.find(vpn);
                if (iter == this->page_table.end()) {
                    /* Page is not resident. */
                    just_swapped_in[j] = true;
                    if (this->free_list.empty()) {
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
                        PhysPageNumber ppn = k->second;
                        if (pair.first.get_usage_time() != invalid_instr) {
                            this->emit_swapout(ppn, evict_vpn);
                        }
                        this->page_table.erase(k);

                        /* Now, swap the desired vpn into the page frame. */
                        if (j != 0 || (current->header.flags & FlagOutputPageFirstUse) == 0) {
                            this->emit_swapin(vpn, ppn);
                        }
                        this->page_table.insert(std::make_pair(vpn, ppn));
                    } else {
                        /* Grab page frame from free list. */
                        PhysPageNumber ppn = this->free_list.back();
                        this->free_list.pop_back();
                        this->page_table.insert(std::make_pair(vpn, ppn));
                        ppns[j] = ppn;
                        if (j != 0 || (current->header.flags & FlagOutputPageFirstUse) == 0) {
                            this->emit_swapin(vpn, ppn);
                        }
                    }
                } else {
                    /* Page is already resident; just use its current frame. */
                    just_swapped_in[j] = false;
                    ppns[j] = iter->second;
                }
            }
            for (std::uint8_t j = 0; j != num_pages; j++) {
                if (just_swapped_in[j]) {
                    this->next_use_heap.insert(ann->slots[j].next_use, vpns[j]);
                } else {
                    this->next_use_heap.decrease_key(ann->slots[j].next_use, vpns[j]);
                }
            }

            phys.restore_page_numbers(*current, ppns.data(), this->page_shift);
            this->emit_instruction(phys);

            InstructionFormat format;
            current = current->next(format);
            ann = ann->next();
        }
    }
}
