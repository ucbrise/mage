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

#include "memprog/scheduling.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include "addr.hpp"
#include "instruction.hpp"
#include "opcode.hpp"
#include "programfile.hpp"

namespace mage::memprog {
    Scheduler::Scheduler(std::string input_file, std::string output_file)
        : input(input_file), output(output_file) {
    }

    Scheduler::~Scheduler() {
    }

    void Scheduler::emit_issue_swapin(StoragePageNumber secondary, PhysPageNumber primary) {
        constexpr std::size_t length = PackedPhysInstruction::size(InstructionFormat::Swap);

        PackedPhysInstruction& phys = this->output.start_instruction(length);
        phys.header.operation = OpCode::IssueSwapIn;
        phys.header.flags = 0;
        phys.header.output = primary;
        phys.swap.storage = secondary;
        this->output.finish_instruction(length);
    }

    void Scheduler::emit_issue_swapout(PhysPageNumber primary, StoragePageNumber secondary) {
        constexpr std::size_t length = PackedPhysInstruction::size(InstructionFormat::Swap);

        PackedPhysInstruction& phys = this->output.start_instruction(length);
        phys.header.operation = OpCode::IssueSwapOut;
        phys.header.flags = 0;
        phys.header.output = primary;
        phys.swap.storage = secondary;
        this->output.finish_instruction(length);
    }

    void Scheduler::emit_page_copy(PhysPageNumber from, PhysPageNumber to) {
        constexpr std::size_t length = PackedPhysInstruction::size(InstructionFormat::Swap);

        PackedPhysInstruction& phys = this->output.start_instruction(length);
        phys.header.operation = OpCode::CopySwap;
        phys.header.flags = 0;
        phys.header.output = to;
        phys.swap.storage = from;
        this->output.finish_instruction(length);
    }

    void Scheduler::emit_finish_swapin(PhysPageNumber ppn) {
        constexpr std::size_t length = PackedPhysInstruction::size(InstructionFormat::Nothing);

        PackedPhysInstruction& finish = this->output.start_instruction();
        finish.header.operation = OpCode::FinishSwapIn;
        finish.header.flags = 0;
        finish.header.output = ppn;
        this->output.finish_instruction(length);
    }

    void Scheduler::emit_finish_swapout(PhysPageNumber ppn) {
        constexpr std::size_t length = PackedPhysInstruction::size(InstructionFormat::Nothing);

        PackedPhysInstruction& finish = this->output.start_instruction();
        finish.header.operation = OpCode::FinishSwapOut;
        finish.header.flags = 0;
        finish.header.output = ppn;
        this->output.finish_instruction(length);
    }

    NOPScheduler::NOPScheduler(std::string input_file, std::string output_file)
        : Scheduler(input_file, output_file) {
        const ProgramFileHeader& header = this->input.get_header();
        this->output.set_page_count(header.num_pages);
        this->output.set_swap_page_count(header.num_pages);
        this->output.set_page_shift(header.page_shift);
    }

    void NOPScheduler::schedule() {
        const ProgramFileHeader& header = this->input.get_header();
        for (std::uint64_t i = 0; i != header.num_instructions; i++) {
            const PackedPhysInstruction& phys = this->input.start_instruction();
            const std::uint8_t* phys_start = reinterpret_cast<const std::uint8_t*>(&phys);
            std::size_t phys_size = phys.size();

            PackedPhysInstruction& into = this->output.start_instruction();
            std::copy(phys_start, phys_start + phys_size, reinterpret_cast<std::uint8_t*>(&into));
            this->output.finish_instruction(phys_size);

            if (phys.header.operation == OpCode::IssueSwapIn) {
                this->emit_finish_swapin(phys.header.output);
            } else if (phys.header.operation == OpCode::IssueSwapOut) {
                this->emit_finish_swapout(phys.header.output);
            }

            this->input.finish_instruction(phys_size);
        }
    }

    BackdatingScheduler::BackdatingScheduler(std::string input_file, std::string output_file, std::uint64_t backdate_gap, std::uint64_t max_in_flight)
        : Scheduler(input_file, output_file), readahead(input_file), gap(backdate_gap) {
        const ProgramFileHeader& header = this->input.get_header();
        this->output.set_page_count(header.num_pages + max_in_flight);
        this->output.set_swap_page_count(header.num_pages);
        this->output.set_page_shift(header.page_shift);
        PhysPageNumber last_page = header.num_pages;
        std::uint64_t i = max_in_flight;
        do {
            i--;
            this->free_pages.push_back(last_page + i);
        } while (i != 0);
    }

    void BackdatingScheduler::emit_issue_swapin(StoragePageNumber secondary, PhysPageNumber primary) {
        assert(this->in_flight_swapins.find(secondary) == this->in_flight_swapins.end());
        assert(this->in_flight_swapouts.find(secondary) == this->in_flight_swapouts.end());
        this->Scheduler::emit_issue_swapin(secondary, primary);
    }

    void BackdatingScheduler::emit_issue_swapout(PhysPageNumber primary, StoragePageNumber secondary) {
        assert(this->in_flight_swapins.find(secondary) == this->in_flight_swapins.end());
        assert(this->in_flight_swapouts.find(secondary) == this->in_flight_swapouts.end());
        this->Scheduler::emit_issue_swapout(primary, secondary);
    }

    bool BackdatingScheduler::allocate_page_frame(PhysPageNumber& ppn) {
        if (!this->free_pages.empty()) {
            ppn = this->free_pages.back();
            this->free_pages.pop_back();
            return true;
        }
        while (!this->in_flight_swapout_queue.empty()) {
            StoragePageNumber spn = this->in_flight_swapout_queue.remove_min().second;
            auto iter = this->in_flight_swapouts.find(spn);
            assert(iter != this->in_flight_swapouts.end());
            ppn = iter->second.second;
            this->in_flight_swapouts.erase(iter);
            this->emit_finish_swapout(ppn);
            return true;
        }
        return false;
    }

    void BackdatingScheduler::deallocate_page_frame(PhysPageNumber ppn) {
        this->free_pages.push_back(ppn);
    }

    void BackdatingScheduler::process_gap_increase(PackedPhysInstruction& phys, InstructionNumber i) {
        if (phys.header.operation == OpCode::IssueSwapIn) {
            /*
             * Check if the most recent swapout to SPN was during the gap.
             * If so, skip the swapout and just copy the value. A swap-in
             * would read stale data. And remember the relevant swapout.
             * If not, try to find a fresh page frame, and do the swap-in
             * there.
             * If you fail to find a fresh page frame, then wait for the oldest
             * pending swapout to complete, and steal its frame.
             * If there are no pending swapouts, then queue this swap-in to
             * happen later, once a free slot opens up.
             */
            PhysPageNumber ppn;
            StoragePageNumber spn = phys.swap.storage;
            auto iter = this->latest_swapout_in_gap.find(spn);
            if (iter != this->latest_swapout_in_gap.end()) {
                // TODO: elide the swapout at iter->second (FOR NOW, SKIP SWAPOUT ELISION)
                // this->scheduled_swapout_elisions.insert(iter->second);
            } else if (this->allocate_page_frame(ppn)) {
                // TODO: initiate the swap-in to this page frame
                this->emit_issue_swapin(spn, ppn);
                this->in_flight_swapins[spn] = ppn;
            } else {
                // TODO: Queue this swap-in to happen later once a free slot open up
                // ppn = phys.header.output;
                // this->queued_swapins.insert(i, std::make_pair(spn, ppn));
            }
        } else if (phys.header.operation == OpCode::IssueSwapOut) {
            /*
             * Record the SPN we're swapping out to during the gap.
             */
            StoragePageNumber spn = phys.swap.storage;
            this->latest_swapout_in_gap[spn] = i;
        }
    }

    void BackdatingScheduler::process_gap_decrease(PackedPhysInstruction& phys, InstructionNumber i) {
        if (phys.header.operation == OpCode::IssueSwapIn) {
            /*
             * Check if a swap in is in flight for this PPN --- if so, add a
             * barrier to make sure we wait appropriately. Then copy in result.
             * Otherwise, check if the corresponding swapout was elided: if so,
             * copy the page from the place it was copied to.
             */
            PhysPageNumber ppn = phys.header.output;
            StoragePageNumber spn = phys.swap.storage;
            auto iter = this->in_flight_swapins.find(spn);
            if (iter != this->in_flight_swapins.end()) {
                this->emit_finish_swapin(iter->second);
                this->emit_page_copy(iter->second, ppn); // allocated when handling swapin
                this->deallocate_page_frame(iter->second);
                this->in_flight_swapins.erase(iter);
            } else if (this->in_flight_swapout_queue.contains(spn)) {
                auto map_iter = this->in_flight_swapouts.find(spn);
                this->emit_page_copy(map_iter->second.second, ppn);
            } else {
                this->emit_issue_swapin(spn, ppn);
                this->emit_finish_swapin(ppn);
                // std::pair<StoragePageNumber, PhysPageNumber> pair(spn, ppn);
                // assert(this->queued_swapins.contains(pair));
                // this->queued_swapins.erase(pair);
            }
        } else if (phys.header.operation == OpCode::IssueSwapOut) {
            /* Maintain the list of swapouts in the gap. */
            StoragePageNumber spn = phys.swap.storage;
            auto iter = this->latest_swapout_in_gap.find(spn);
            assert(iter != this->latest_swapout_in_gap.end());
            if (iter->second == i) {
                this->latest_swapout_in_gap.erase(iter);
            }

            /*
             * If the swapout was skipped (we would have recorded this) then
             * copy the page instead of swapping it out.
             * Otherwise, find a free frame and initiate the swapout. It will
             * be reaped when we run out of frames and a swap-in needs a fresh
             * frame (and, hopefully, enough time has passed...).
             * If no free frame can be found, then we have to swap out
             * synchronously.
             */
            PhysPageNumber ppn;
            if (this->allocate_page_frame(ppn)) {
                this->emit_page_copy(phys.header.output, ppn);
                if (in_flight_swapouts.contains(spn)) {
                    /*
                     * This could happen if we swap out to a storage page, then
                     * manage to copy from the ppn without reading from disk.
                     * We still have to perform the write in that case, in case
                     * the storage page is read in the future, but if it isn't,
                     * we'll end up here.
                     */
                    auto iter = this->in_flight_swapouts.find(spn);
                    this->emit_finish_swapout(iter->second.second);
                    this->in_flight_swapout_queue.erase(spn);
                    this->in_flight_swapouts.erase(iter);
                }
                this->emit_issue_swapout(ppn, spn);
                this->in_flight_swapouts[spn] = std::make_pair(i, ppn);
                this->in_flight_swapout_queue.insert(i, spn);
            } else {
                this->emit_issue_swapout(phys.header.output, spn);
                this->emit_finish_swapout(phys.header.output);
            }
        } else {
            /* Copy instruction to output. */
            const std::uint8_t* phys_start = reinterpret_cast<const std::uint8_t*>(&phys);
            std::size_t phys_size = phys.size();
            PackedPhysInstruction& into = this->output.start_instruction();
            std::copy(phys_start, phys_start + phys_size, reinterpret_cast<std::uint8_t*>(&into));
            this->output.finish_instruction(phys_size);
        }
    }

    void BackdatingScheduler::schedule() {
        std::uint64_t num_instructions = this->input.get_header().num_instructions;
        std::uint64_t i, j;

        // First, create a gap
        for (i = 0; i != this->gap && i != num_instructions; i++) {
            PackedPhysInstruction& phys = this->readahead.start_instruction();
            this->process_gap_increase(phys, i);
            this->readahead.finish_instruction(phys.size());
        }

        // Process the remaining instructions
        for (j = 0; i != num_instructions; i++, j++) {
            PackedPhysInstruction& current = this->input.start_instruction();
            this->process_gap_decrease(current, i);
            this->input.finish_instruction(current.size());

            PackedPhysInstruction& phys = this->readahead.start_instruction();
            this->process_gap_increase(phys, i);
            this->readahead.finish_instruction(phys.size());
        }

        // Drain the gap
        for (; j != num_instructions; j++) {
            PackedPhysInstruction& current = this->input.start_instruction();
            this->process_gap_decrease(current, i);
            this->input.finish_instruction(current.size());
        }

    }
}
