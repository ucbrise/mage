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

#ifndef MAGE_MEMPROG_SCHEDULING_HPP_
#define MAGE_MEMPROG_SCHEDULING_HPP_

#include <cstdlib>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include "addr.hpp"
#include "instruction.hpp"
#include "programfile.hpp"
#include "util/prioqueue.hpp"

namespace mage::memprog {
    class Scheduler {
    public:
        Scheduler(std::string input_file, std::string output_file);
        virtual ~Scheduler();

        virtual void schedule() = 0;

    protected:
        void emit_issue_swapin(StoragePageNumber secondary, PhysPageNumber primary);
        void emit_issue_swapout(PhysPageNumber primary, StoragePageNumber secondary);
        void emit_page_copy(PhysPageNumber from, PhysPageNumber to);
        void emit_finish_swapin(PhysPageNumber ppn);
        void emit_finish_swapout(PhysPageNumber ppn);

        PhysProgramFileReader input;
        PhysProgramFileWriter output;
    };

    class NOPScheduler : public Scheduler {
    public:
        NOPScheduler(std::string input_file, std::string output_file);

        void schedule() override;
    };

    class BackdatingScheduler : public Scheduler {
    public:
        BackdatingScheduler(std::string input_file, std::string output_file, std::uint64_t backdate_gap, std::uint64_t max_in_flight);

        std::uint64_t get_num_allocation_failures() const;
        std::uint64_t get_num_synchronous_swapins() const;

        bool allocate_page_frame(PhysPageNumber& ppn);
        void deallocate_page_frame(PhysPageNumber ppn);

        void emit_issue_swapin(StoragePageNumber secondary, PhysPageNumber primary);
        void emit_issue_swapout(PhysPageNumber primary, StoragePageNumber secondary);

        void process_gap_increase(PackedPhysInstruction& phys, InstructionNumber i);
        void process_gap_decrease(PackedPhysInstruction& phys, InstructionNumber i);

        void schedule() override;

    private:
        PhysProgramFileReader readahead;
        // util::PriorityQueue<InstructionNumber, std::pair<StoragePageNumber, PhysPageNumber>> queued_swapins;
        std::unordered_map<StoragePageNumber, PhysPageNumber> finished_swapout_elisions;
        std::unordered_set<InstructionNumber> scheduled_swapout_elisions;
        std::unordered_map<StoragePageNumber, PhysPageNumber> in_flight_swapins;
        std::unordered_map<StoragePageNumber, InstructionNumber> latest_swapout_in_gap;
        util::PriorityQueue<InstructionNumber, StoragePageNumber> in_flight_swapout_queue;
        std::unordered_map<StoragePageNumber, std::pair<InstructionNumber, PhysPageNumber>> in_flight_swapouts;
        std::uint64_t gap;
        std::vector<PhysPageNumber> free_pages;

        std::uint64_t num_allocation_failures;
        std::uint64_t num_synchronous_swapins;
    };
}

#endif
