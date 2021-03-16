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

#ifndef MAGE_MEMPROG_REPLACEMENT_HPP_
#define MAGE_MEMPROG_REPLACEMENT_HPP_

#include <string>
#include "addr.hpp"
#include "instruction.hpp"
#include "memprog/annotation.hpp"
#include "opcode.hpp"
#include "platform/memory.hpp"
#include "programfile.hpp"
#include "util/prioqueue.hpp"

/**
 * @file memprog/replacement.hpp
 * @brief Replacement stage for MAGE's planner
 *
 * The replacement module consumes virtual bytecode from the placement phase
 * and outputs physical bytecode.
 */

namespace mage::memprog {
    /**
     * @brief Abstract class for a replacement module in MAGE's planner.
     *
     * This includes standard functionality that most replacement strategies
     * are expected to share.
     */
    class Allocator {
    public:
        /**
         * @brief Initiailzes an @p Allocator that computes replacement for
         * the specified memory constraints and writes the resulting physical
         * bytecode to the specified file.
         *
         * @param output_file The name of the file to which to write the
         * physical bytecode.
         * @param num_page_frames The number of physical pages available.
         * @param page_shift Base-2 logarithm of the page size.
         */
        Allocator(std::string output_file, PhysPageNumber num_page_frames, PageShift page_shift);

        /**
         * @brief Destructor.
         */
        virtual ~Allocator();

        /**
         * @brief Sets the page size used to compute replacement.
         *
         * @param shift Base-2 logarithm of the page size.
         */
        void set_page_shift(PageShift shift);

        /**
         * @brief Computes replacement and writes out the resulting physical
         * bytecode.
         */
        virtual void allocate() = 0;

        /**
         * @brief Obtains the number of times that the emitted physical
         * bytecode transfers a page from memory to storage.
         *
         * @return The number of "swap out" instructions emitted.
         */
        std::uint64_t get_num_swapouts() const;

        /**
         * @brief Obtains the number of times that the emitted physical
         * bytecode transfers a page from storage to memory.
         *
         * @return The number of "swap in" instructions emitted.
         */
        std::uint64_t get_num_swapins() const;

        /**
         * @brief Obtains the amount of storage space, in page frames, used by
         * the emitted byte code.
         *
         * @return The number of unique storage page frames used by the
         * instructions emitted.
         */
        StoragePageNumber get_num_storage_frames() const;

    protected:
        /**
         * @brief Emits one or more instructions to the physical bytecode to
         * swap out a page from memory to storage, first waiting for any
         * outstanding operations on the page to finish.
         *
         * If the page is the target of one or more outstanding network
         * receive operations, then network barriers are emitted as needed to
         * ensure that any such operations will have completed before the page
         * is swapped out.
         *
         * @param primary The physical page number in memory from which to read
         * the page.
         * @param secondary The frame number in storage to which to write the
         * page.
         */
        void emit_swapout(PhysPageNumber primary, StoragePageNumber secondary);

        /**
         * @brief Emits one or more instructions to the physical bytecode to
         * swap in a page from storage to memory, first waiting for any
         * outstanding operations on the page to finish.
         *
         * @param secondary The frame number in storage from which to read the
         * page.
         * @param primary The physical page number in memory to which to write
         * the page.
         */
        void emit_swapin(StoragePageNumber secondary, PhysPageNumber primary);

        /**
         * @brief Update the allocator's bookkeeping of outstanding network
         * operations initiated or completed by the specified instruction.
         *
         * @param phys A reference to the provided instruction.
         */
        void update_network_state(const PackedPhysInstruction& phys);

        /**
         * @brief Allocates a new page frame in storage to which a page can be
         * swapped out.
         *
         * The storage frame may either be fresh (never-before-used) storage
         * frame, or it may be a frame that was allocated earlier and then
         * freed.
         *
         * @return The frame number in storage of the allocated page frame.
         */
        StoragePageNumber alloc_storage_frame() {
            if (this->free_storage_frames.empty()) {
                return this->next_storage_frame++;
            } else {
                StoragePageNumber spn = this->free_storage_frames.back();
                this->free_storage_frames.pop_back();
                return spn;
            }
        }

        /**
         * @brief Deallocates a page frame in storage to which a page can be
         * swapped out.
         *
         * @pre The MAGE-physical page frame corresponding to @p spn should
         * have been previously allocated using @p alloc_storage_frame.
         * @param spn The page frame in storage to deallocate.
         */
        void free_storage_frame(StoragePageNumber spn) {
            this->free_storage_frames.push_back(spn);
        }

        /**
         * @brief Returns true if any MAGE-physical page frames in memory are
         * currently unallocated.
         *
         * If this returns true, then a page frame can be allocated using
         * @p alloc_page_frame.
         *
         * @return True if at least one MAGE-physical page frame in memory is
         * not allocated.
         */
        bool page_frame_available() const {
            return !this->free_page_frames.empty();
        }

        /**
         * @brief Allocates a MAGE-physical page frame in memory and returns is
         * physical page number.
         *
         * @pre At least one MAGE-physical page frame is not allocated (i.e.,
         * @p page_frame_available() returns true).
         * @return The physical page number of the allocated MAGE-physical page
         * frame.
         */
        PhysPageNumber alloc_page_frame() {
            PhysPageNumber ppn = this->free_page_frames.back();
            this->free_page_frames.pop_back();
            this->pages_end = std::max(this->pages_end, ppn + 1);
            return ppn;
        }

        /**
         * @brief Deallocates a MAGE-physical page frame.
         *
         * @pre The MAGE-physical page frame corresponding to @p spn should
         * have been previously allocated using @p alloc_storage_frame.
         * @param ppn The physical page number of the MAGE-physical page frame
         * to deallocate.
         */
        void free_page_frame(PhysPageNumber ppn) {
            this->free_page_frames.push_back(ppn);
        }

        PhysProgramFileWriter phys_prog;
        PageShift page_shift;

    private:
        std::vector<PhysPageNumber> free_page_frames;
        std::vector<StoragePageNumber> free_storage_frames;
        StoragePageNumber next_storage_frame;
        PhysPageNumber pages_end;

        /*
         * Network information so that we can make sure to steal pages used for
         * async network operations. The index into both vectors is the
         * WorkerID.
         */
        std::vector<std::unordered_set<PhysPageNumber>> pending_receive_ops;
        std::vector<bool> buffered_send_ops;

        /* Keeps track of the number of swaps performed in the allocation. */
        std::uint64_t num_swapouts;
        std::uint64_t num_swapins;
    };

    /**
     * @brief Represents the "score" of a page in the context of Belady's
     * theoretically optimal paging algorithm.
     *
     * Instances of this score are related by the "<" and "==" operators, where
     * A < B means that A is a better candidate for eviction.
     */
    class BeladyScore {
    public:
        /**
         * @brief Default constructor, which creates a score corresponding to
         * the most unfavorable candidate for eviction.
         */
        BeladyScore() : BeladyScore(0) {
        }

        /**
         * @brief Constructs a @p BeladyScore instance based on the provided
         * time of next access.
         *
         * @param The number (index) of the instruction at which the described
         * page will next be accessed.
         */
        BeladyScore(InstructionNumber usage_time) : usage(usage_time) {
        }

        /**
         * @brief Obtains the time of next access.
         *
         * @return The number (index) of the instruction at which the described
         * page will next be accessed.
         */
        InstructionNumber get_usage_time() const {
            return this->usage;
        }

        /**
         * @brief Compares this score to the specified score.
         *
         * @param other A reference to the score to which to compare this one.
         * @return True if this score indicates a more favorable eviction
         * candidate than @p other.
         */
        bool operator <(const BeladyScore& other) const {
            return this->usage > other.usage;
        }

        /**
         * @brief Checks if this score is equal to the specified score.
         *
         * @param other A reference to the score with which this score is
         * checked for equality.
         * @return True if this score is equal to @p other.
         */
        bool operator ==(const BeladyScore& other) const {
            return this->usage == other.usage;
        }

    private:
        InstructionNumber usage;
    };

    /**
     * @brief An entry in the page table maintained by MAGE's replacement
     * phase.
     */
    struct PageTableEntry {
        bool spn_allocated;
        bool resident;
        bool dirty;
        StoragePageNumber spn : storage_address_bits;
        PhysPageNumber ppn : physical_address_bits;
    } __attribute__((packed));

    /**
     * @brief The Replacement module used by MAGE's default planning pipeline.
     *
     * This Replacement module uses Belady's theoretically-optimal paging
     * algorithm (MIN) to optimize for storage bandwidth.
     */
    class BeladyAllocator : public Allocator {
    public:
        /**
         * @brief Creates a @p BeladyAllocator instance that consumes the
         * specified virtual bytecode and annotations, computes a physical
         * bytecode using Belady's theoretically optimal paging algorithm,
         * and writes the resulting physical bytecode to a file with the
         * specified name.
         *
         * @param output_file The name of the file to which to write the
         * resulting physical bytecode.
         * @param virtual_program_file The name of the file from which to read
         * the virtual bytecode, which is expected to be reverse-iterable
         * (e.g., written using a BufferedFileWriter with
         * backwards_readable == true).
         * @param annotations_file The name of the file from which to read the
         * next-use annotations for the virtual bytecode.
         * @param num_page_frames The number of physical pages available.
         * @param shift Base-2 logarithm of the page size.
         */
        BeladyAllocator(std::string output_file, std::string virtual_program_file, std::string annotations_file, PhysPageNumber num_page_frames, PageShift shift);

        void allocate() override;

    private:
        std::unordered_map<VirtPageNumber, PageTableEntry> page_table;
        util::PriorityQueue<BeladyScore, VirtPageNumber> next_use_heap;
        VirtProgramFileReader virt_prog;
        util::BufferedReverseFileReader<true> annotations;
    };
}

#endif
