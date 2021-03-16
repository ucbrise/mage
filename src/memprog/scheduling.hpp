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

/**
 * @file memprog/scheduling.hpp
 * @brief Scheduling stage for MAGE's planner
 *
 * The scheduling module consumes physical bytecode from the replacement phase
 * and outputs a memory program.
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
    /**
     * @brief Abstract class for a scheduling module in MAGE's planner.
     *
     * This includes standard functionality that most scheduling strategies
     * are expected to share.
     */
    class Scheduler {
    public:
        /**
         * @brief Initializes a @p Scheduler that reads physical bytecode from
         * the specified input file and writes a memory program to the
         * specified output file.
         *
         * @param input_file The name of the file from which to read the
         * physical bytecode.
         * @param output_file The name of the file to which to write the memory
         * program.
         */
        Scheduler(std::string input_file, std::string output_file);

        /**
         * @brief Destructor.
         */
        virtual ~Scheduler();

        /**
         * @brief Runs the scheduling algorithm and outputs the resulting
         * memory program.
         */
        virtual void schedule() = 0;

    protected:
        /**
         * @brief Emits an "issue swap in" instruction to the memory program.
         *
         * @param secondary The frame number in storage from where to read the
         * page.
         * @param primary The physical page number of the page frame in memory
         * to which to write the page.
         */
        void emit_issue_swapin(StoragePageNumber secondary, PhysPageNumber primary);

        /**
         * @brief Emits an "issue swap out" instruction to the memory program.
         *
         * @param primary The physical page number of the page frame in memory
         * from where to read the page.
         * @param secondary The frame number in storage to which to write the
         * page.
         */
        void emit_issue_swapout(PhysPageNumber primary, StoragePageNumber secondary);

        /**
         * @brief Emits a "page copy" instruction to the memory program.
         *
         * @param from The physical page number of the page frame in memory
         * from where to read the page.
         * @param to The physical page number of the page frame in memory to
         * which to write the page.
         */
        void emit_page_copy(PhysPageNumber from, PhysPageNumber to);

        /**
         * @brief Emits a "finish swap in" instruction to the memory program.
         *
         * This instruction will cause the engine to wait for a "swap in"
         * operation to complete before continuing execution.
         *
         * @param ppn The physical page number of the page frame in memory of
         * an outstanding "swap in" operation.
         */
        void emit_finish_swapin(PhysPageNumber ppn);

        /**
         * @brief Emits a "finish swap out" instruction to the memory program.
         *
         * This instruction will cause the engine to wait for a "swap out"
         * operation to complete before continuing execution.
         *
         * @param ppn The physical page number of the page frame in memory of
         * an outstanding "swap out" operation.
         */
        void emit_finish_swapout(PhysPageNumber ppn);

        PhysProgramFileReader input;
        PhysProgramFileWriter output;
    };

    /**
     * @brief A simple baseline scheduler that stalls on each swap operation.
     *
     * This scheduler does not intelligently schedule anything; it does the
     * minimum possible work to transform physical bytecode output by the
     * replacement phase into a valid memory program.
     */
    class NOPScheduler : public Scheduler {
    public:
        /**
         * @brief Initializes a @p NOPScheduler that reads physical bytecode
         * from the specified input file and writes a memory program to the
         * specified output file.
         *
         * @param input_file The name of the file from which to read the
         * physical bytecode.
         * @param output_file The name of the file to which to write the memory
         * program.
         */
        NOPScheduler(std::string input_file, std::string output_file);

        void schedule() override;
    };

    /**
     * @brief The scheduling module used by MAGE's default planning pipeline.
     *
     * This scheduler attempts to prefetch each "swap in" operation and perform
     * each "swap out" operation as asynchronously as possible. The algorithm
     * has two parameters: (1) the lookahead, and (2) the prefetch buffer size.
     * The lookahead determines by how many instructions to prefetch each
     * "swap in" operation; "swap out" operations are committed at the latest
     * moment possible. The prefetch buffer is used as scratch space at runtime
     * to eliminate data dependencies for prefetching and asynchronous
     * eviction. The size of the prefetch buffer limits the number of
     * current swap operations in the final plan. The maximum number of
     * concurrent swap operations is the number of page frames in the prefetch
     * buffer plus one.
     */
    class BackdatingScheduler : public Scheduler {
    public:
        /**
         * @brief Initializes a @p BackdatingScheduler that reads physical
         * bytecode from the specified input file and writes a memory program
         * to the specified output file.
         *
         * @param input_file The name of the file from which to read the
         * physical bytecode.
         * @param output_file The name of the file to which to write the memory
         * program.
         * @param lookahead The number of instructions by which to prefetch
         * each "swap in" operation.
         * @param prefetch_buffer_size The number of extra MAGE-physical page
         * frames (beyond those used in the replacement phase) used for
         * scheduling swap operations.
         */
        BackdatingScheduler(std::string input_file, std::string output_file, std::uint64_t lookahead, std::uint32_t prefetch_buffer_size);

        /**
         * @brief Obtains the number of times the scheduler was unable to
         * allocate a page from from the prefetch buffer.
         *
         * If allocating a page frame from the prefetch buffer fails, then the
         * scheduler falls back to a synchronous swap operation to bypass the
         * prefetch buffer. The one exception to this is the case where the
         * scheduler finds a way to eliminate the swap operation altogether.
         *
         * @return The number of allocation failures that occurred while
         * scheduling swap operations.
         */
        std::uint64_t get_num_allocation_failures() const;

        /**
         * @brief Obtains the number of times the scheduler failed to prefetch
         * a "swap in" operation and had to perform the operation
         * synchronously.
         *
         * @return The number of times that the emitted memory program swaps in
         * a page synchronously (i.e., with no intervening instructions
         * between the "issue swap in" and "finish swap in" directives).
         */
        std::uint64_t get_num_synchronous_swapins() const;

        /**
         * @brief Allocates a page frame from the prefetch buffer.
         *
         * @param[out] ppn Populated with the physical page number of the
         * allocated page, if the allocation is sucessful. If the allocation is
         * not successful, @p ppn is left uninitialized.
         * @return True if the allocation is successful, and false if it is
         * not successful.
         */
        bool allocate_page_frame(PhysPageNumber& ppn);

        /**
         * @brief Deallocates a page frame in the prefetch buffer, making it
         * available for future allocation requests.
         *
         * @param ppn The physical page number of the page frame to deallocate.
         */
        void deallocate_page_frame(PhysPageNumber ppn);

        /**
         * @brief Emits an "issue swap in" instruction to the memory program.
         *
         * @param secondary The frame number in storage from where to read the
         * page.
         * @param primary The physical page number of the page frame in memory
         * to which to write the page.
         */
        void emit_issue_swapin(StoragePageNumber secondary, PhysPageNumber primary);

        /**
         * @brief Emits an "issue swap out" instruction to the memory program.
         *
         * @param primary The physical page number of the page frame in memory
         * from where to read the page.
         * @param secondary The frame number in storage to which to write the
         * page.
         */
        void emit_issue_swapout(PhysPageNumber primary, StoragePageNumber secondary);

        /**
         * @brief Processes an instruction at the front end of the "gap"
         * between the point where pages are prefetched and the point at which
         * they are needed.
         *
         * Each instruction of the physical bytecode is processed twice; once
         * at the front of the gap (by this function) and again at the back of
         * the gap (by @p process_gap_decrease).
         *
         * @param phys A reference to the instruction to process.
         * @param i The number (index) of the instruction referenced by
         * @p phys.
         */
        void process_gap_increase(PackedPhysInstruction& phys, InstructionNumber i);

        /**
         * @brief Processes an instruction at the back end of the "gap" between
         * the point where pages are prefetched and the point at which they are
         * needed.
         *
         * Each instruction of the physical bytecode is processed twice; once
         * at the front of the gap (by @p process_gap_increase) and again at
         * the back of the gap (by this function).
         *
         * @param phys A reference to the instruction to process.
         * @param i The number (index) of the instruction referenced by
         * @p phys.
         */
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
        InstructionNumber current_instruction;

        std::uint64_t num_allocation_failures;
        std::uint64_t num_synchronous_swapins;
    };
}

#endif
