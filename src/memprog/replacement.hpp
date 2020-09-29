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

namespace mage::memprog {
    class Allocator {
    public:
        Allocator(std::string output_file, PhysPageNumber num_page_frames, PageShift page_shift);
        virtual ~Allocator();

        void set_page_shift(PageShift shift);

        virtual void allocate() = 0;

        std::uint64_t get_num_swapouts() const;
        std::uint64_t get_num_swapins() const;
        StoragePageNumber get_num_storage_frames() const;

    protected:
        void emit_swapout(PhysPageNumber primary, StoragePageNumber secondary);
        void emit_swapin(StoragePageNumber secondary, PhysPageNumber primary);

        void update_network_state(const PackedPhysInstruction& phys);

        StoragePageNumber alloc_storage_frame() {
            if (this->free_storage_frames.empty()) {
                return this->next_storage_frame++;
            } else {
                StoragePageNumber spn = this->free_storage_frames.back();
                this->free_storage_frames.pop_back();
                return spn;
            }
        }

        void free_storage_frame(StoragePageNumber spn) {
            this->free_storage_frames.push_back(spn);
        }

        bool page_frame_available() const {
            return !this->free_page_frames.empty();
        }

        PhysPageNumber alloc_page_frame() {
            PhysPageNumber ppn = this->free_page_frames.back();
            this->free_page_frames.pop_back();
            this->pages_end = std::max(this->pages_end, ppn + 1);
            return ppn;
        }

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

    class BeladyScore {
    public:
        BeladyScore() : BeladyScore(0) {
        }

        BeladyScore(InstructionNumber usage_time) : usage(usage_time) {
        }

        InstructionNumber get_usage_time() const {
            return this->usage;
        }

        bool operator <(const BeladyScore& other) const {
            return this->usage > other.usage;
        }

        bool operator ==(const BeladyScore& other) const {
            return this->usage == other.usage;
        }

    private:
        InstructionNumber usage;
    };

    struct PageTableEntry {
        bool spn_allocated;
        bool resident;
        bool dirty;
        StoragePageNumber spn : storage_address_bits;
        PhysPageNumber ppn : physical_address_bits;
    } __attribute__((packed));;

    class BeladyAllocator : public Allocator {
    public:
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
