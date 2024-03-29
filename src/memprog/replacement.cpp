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
#include "addr.hpp"
#include "instruction.hpp"
#include "memprog/annotation.hpp"
#include "opcode.hpp"

namespace mage::memprog {
    Allocator::Allocator(std::string output_file, PhysPageNumber num_page_frames, PageShift shift)
        : next_storage_frame(0), pages_end(0), page_shift(shift), num_swapouts(0), num_swapins(0), phys_prog(output_file, 0, num_page_frames) {
        this->free_page_frames.reserve(num_page_frames);
        PhysPageNumber curr = num_page_frames;
        do {
            curr--;
            this->free_page_frames.push_back(curr);
        } while (curr != 0);
    }

    Allocator::~Allocator() {
        this->phys_prog.set_page_count(this->pages_end);
        this->phys_prog.set_swap_page_count(this->next_storage_frame);
    }

    void Allocator::set_page_shift(PageShift shift) {
        this->phys_prog.set_page_shift(shift);
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

    void Allocator::emit_swapout(PhysPageNumber primary, StoragePageNumber secondary) {
        /*
         * Before swapping out this page, make sure to finish any outstanding
         * network receives for this page. Otherwise, the receive may complete
         * after we've swapped out the page overwriting whatever else is at the
         * physical page frame...
         * But this is prone to deadlock, because the data might still be in
         * the sender's buffer, causing us to block, and the sender may also
         * block in the same way, and so on, in a cycle. So, we should first
         * flush any output buffers, and only then wait for any outstanding
         * data to arrive.
         */
        bool flushed_send_buffers = false;
        for (WorkerID i = 0; i != this->pending_receive_ops.size(); i++) {
            std::unordered_set<PhysPageNumber>& pending = this->pending_receive_ops[i];
            if (pending.contains(primary)) {
                constexpr std::size_t control_length = PackedPhysInstruction::size(InstructionFormat::Control);
                if (!flushed_send_buffers) {
                    for (WorkerID j = 0; j != this->buffered_send_ops.size(); j++) {
                        if (this->buffered_send_ops[j]) {
                            PackedPhysInstruction& phys = this->phys_prog.start_instruction(control_length);
                            phys.header.operation = OpCode::NetworkFinishSend;
                            phys.header.flags = 0;
                            phys.control.data = j;
                            this->phys_prog.finish_instruction(control_length);
                            this->buffered_send_ops[j] = false;
                        }
                    }
                    flushed_send_buffers = true;
                }
                PackedPhysInstruction& phys = this->phys_prog.start_instruction(control_length);
                phys.header.operation = OpCode::NetworkFinishReceive;
                phys.header.flags = 0;
                phys.control.data = i;
                this->phys_prog.finish_instruction(control_length);
                pending.clear();
            }
        }

        constexpr std::size_t length = PackedPhysInstruction::size(InstructionFormat::Swap);

        PackedPhysInstruction& phys = this->phys_prog.start_instruction(length);
        phys.header.operation = OpCode::IssueSwapOut;
        phys.header.flags = 0;
        phys.swap.memory = primary;
        phys.swap.storage = secondary;
        this->phys_prog.finish_instruction(length);

        this->num_swapouts++;
    }

    void Allocator::emit_swapin(StoragePageNumber secondary, PhysPageNumber primary) {
        constexpr std::size_t length = PackedPhysInstruction::size(InstructionFormat::Swap);

        PackedPhysInstruction& phys = this->phys_prog.start_instruction(length);
        phys.header.operation = OpCode::IssueSwapIn;
        phys.header.flags = 0;
        phys.swap.memory = primary;
        phys.swap.storage = secondary;
        this->phys_prog.finish_instruction(length);

        this->num_swapins++;
    }

    void Allocator::update_network_state(const PackedPhysInstruction& phys) {
        WorkerID other;
        switch (phys.header.operation) {
        case OpCode::NetworkPostReceive:
            other = phys.constant.constant;
            if (other + 1 > this->pending_receive_ops.size()) {
                this->pending_receive_ops.resize(other + 1);
            }
            this->pending_receive_ops[other].insert(pg_num(phys.constant.output, this->page_shift));
            break;
        case OpCode::NetworkFinishReceive:
            other = phys.control.data;
            if (other + 1 > this->pending_receive_ops.size()) {
                this->pending_receive_ops.resize(other + 1);
            }
            this->pending_receive_ops[other].clear();
            break;
        case OpCode::NetworkBufferSend:
            other = phys.constant.constant;
            if (other + 1 > this->buffered_send_ops.size()) {
                this->buffered_send_ops.resize(other + 1);
            }
            this->buffered_send_ops[other] = true;
            break;
        case OpCode::NetworkFinishSend:
            other = phys.control.data;
            if (other + 1 > this->buffered_send_ops.size()) {
                this->buffered_send_ops.resize(other + 1);
            }
            this->buffered_send_ops[other] = false;
            break;
        default:
            break;
        }
    }

    BeladyAllocator::BeladyAllocator(std::string output_file, std::string virtual_program_file, std::string annotations_file, PhysPageNumber num_page_frames, PageShift shift)
        : Allocator(output_file, num_page_frames, shift), virt_prog(virtual_program_file.c_str()), annotations(annotations_file.c_str()) {
        this->set_page_shift(this->virt_prog.get_header().page_shift);
    }

    void BeladyAllocator::allocate(util::ProgressBar* progress_bar) {
        this->virt_prog.set_progress_bar(progress_bar);
        InstructionNumber num_instructions = this->virt_prog.get_header().num_instructions;
        std::array<bool, 5> just_swapped_in;
        std::array<PhysPageNumber, 5> ppns;
        std::array<VirtPageNumber, 5> vpns;
        for (InstructionNumber i = 0; i != num_instructions; i++) {
            PackedVirtInstruction& current = this->virt_prog.start_instruction();
            OpInfo info(current.header.operation);
            std::uint8_t num_pages = current.store_page_numbers(vpns.data(), this->page_shift);
            std::size_t ann_size;
            Annotation& ann = this->annotations.read<Annotation>(ann_size);
            assert(num_pages == ann.header.num_pages);
            for (std::uint8_t j = 0; j != num_pages; j++) {
                VirtPageNumber vpn = vpns[j];
                bool dirties_page = (j == 0) && info.has_variable_output();

                auto iter = this->page_table.find(vpn);
                if (iter != this->page_table.end() && iter->second.resident) {
                    /* Page is already resident; just use its current frame. */
                    just_swapped_in[j] = false;
                    PageTableEntry& pte = iter->second;
                    ppns[j] = pte.ppn;
                    pte.dirty |= dirties_page;

                    /*
                     * If page is never used again, remove it from page table.
                     * Doing this later would require another hash table lookup
                     * and it's safe to do it now because vpns does not contain
                     * any repeat VPNs. We mark the PPN as free after
                     * processing all VPNs for this instruction, so that we
                     * don't accidentally swap two different virtual pages into
                     * the same physical page frame.
                     */
                    if (ann.slots[j].next_use == invalid_instr) {
                        if (pte.spn_allocated) {
                            this->free_storage_frame(pte.spn);
                        }
                        this->page_table.erase(iter);
                        this->next_use_heap.erase(vpn);
                    }
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
                        assert(pair.first.get_usage_time() != invalid_instr);
                        evict_pte.resident = false;
                        if (evict_pte.dirty) {
                            evict_pte.dirty = false;
                            if (!evict_pte.spn_allocated) {
                                evict_pte.spn = this->alloc_storage_frame();
                                evict_pte.spn_allocated = true;
                            }
                            this->emit_swapout(ppn, evict_pte.spn);
                        }
                    }

                    /* Now, swap the desired vpn into the page frame. */
                    if (iter == this->page_table.end()) {
                        /*
                         * First use of this VPN, so no need to swap it in.
                         * Just update the page table. If the page is never
                         * used again, skip updating the page table (see the
                         * comment above: "If page is never used again...").
                         */
                        assert(j == 0 && (current.header.flags & FlagOutputPageFirstUse) != 0);
                        if (ann.slots[j].next_use != invalid_instr) {
                            PageTableEntry pte;
                            pte.resident = true;
                            pte.spn_allocated = false;
                            pte.dirty = true; // we're guaranteed to be an output on first use
                            pte.ppn = ppn;
                            auto rv = this->page_table.insert(std::make_pair(vpn, pte));
                            assert(rv.second);
                            iter = rv.first;
                        }
                    } else {
                        /* Swap the desired VPN into the page frame and update
                         * the page table. If the page is never used again,
                         * remove the page table entry (see the comment above:
                         * "If page is never used again...").
                         */
                        PageTableEntry& pte = iter->second;
                        assert(!pte.resident);
                        assert(pte.spn_allocated);
                        this->emit_swapin(pte.spn, ppn);
                        if (ann.slots[j].next_use == invalid_instr) {
                            this->page_table.erase(iter);
                        } else {
                            pte.dirty |= dirties_page;
                            pte.resident = true;
                            pte.ppn = ppn;
                        }
                    }
                    ppns[j] = ppn;
                }
            }

            PackedPhysInstruction& phys = this->phys_prog.start_instruction();
            phys.header.operation = current.header.operation;
            phys.no_args.width = current.no_args.width;
            phys.header.flags = current.header.flags;
            phys.restore_page_numbers(current, ppns.data(), this->page_shift);
            this->update_network_state(phys);
            this->phys_prog.finish_instruction(phys.size());

            /*
             * Possible optimization: try to avoid swapping out a page for which
             * an asynchronous receive was recently issued. It's unclear if
             * this would provide benefit; I'll need to do benchmarks first.
             *
             * The idea is that if we keep issuing async receives and
             * immediately swapping out the page we're receiving into, we have
             * to stall, waiting for the receive to finish, before swapping out
             * the page. So perhaps the replacement should take async receives
             * into account by marking the page as "pinned" or "busy" for a
             * short time after the async receive is issued, to give the
             * data some time to arrive, so we stall for less/no time waiting
             * for it to arrive before swapping out the page.
             */

            for (std::uint8_t j = 0; j != num_pages; j++) {
                InstructionNumber next_use = ann.slots[j].next_use;
                /*
                 * If the page is never used again, then the above code has
                 * already removed its page table entry. Here, we just mark its
                 * PPN as free so that on future instructions the PPN can be
                 * reclaimed. (We could also put the code to remove the PTE
                 * here, but that would require an additional lookup in the
                 * page table).
                 */
                if (next_use == invalid_instr) {
                    this->free_page_frame(ppns[j]);
                } else if (just_swapped_in[j]) {
                    this->next_use_heap.insert(next_use, vpns[j]);
                } else {
                    this->next_use_heap.decrease_key(next_use, vpns[j]);
                }
            }

            this->virt_prog.finish_instruction(current.size());
        }
    }
}
