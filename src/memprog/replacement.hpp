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
#include "memprog/programfile.hpp"
#include "opcode.hpp"
#include "platform/memory.hpp"
#include "util/prioqueue.hpp"

namespace mage::memprog {
    class Allocator {
    public:
        Allocator(std::string output_file, PhysPageNumber num_page_frames);
        virtual ~Allocator();

        virtual void allocate() = 0;

        std::uint64_t get_num_swapouts() const;
        std::uint64_t get_num_swapins() const;
        StoragePageNumber get_num_storage_frames() const;

    protected:
        StoragePageNumber emit_swapout(PhysPageNumber primary);
        void emit_swapin(StoragePageNumber secondary, PhysPageNumber primary);

        bool page_frame_available() const {
            return !this->free_page_frames.empty();
        }

        PhysPageNumber alloc_page_frame() {
            PhysPageNumber ppn = this->free_page_frames.back();
            this->free_page_frames.pop_back();
            return ppn;
        }

        void free_page_frame(PhysPageNumber ppn) {
            this->free_page_frames.push_back(ppn);
        }

        PhysProgramFileWriter phys_prog;

    private:
        std::vector<PhysPageNumber> free_page_frames;
        std::vector<StoragePageNumber> free_storage_frames;
        StoragePageNumber next_storage_frame;

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

        bool operator<(const BeladyScore& other) const {
            return this->usage > other.usage;
        }

        bool operator==(const BeladyScore& other) const {
            return this->usage == other.usage;
        }

    private:
        InstructionNumber usage;
    };

    struct PageTableEntry {
        union {
            PhysPageNumber ppn : physical_address_bits;
            StoragePageNumber spn : storage_address_bits;
            std::uint64_t pad : 56;
        } __attribute__((packed));
        bool resident;
    };

    class BeladyAllocator : public Allocator {
    public:
        BeladyAllocator(std::string output_file, std::string virtual_program_file, std::string annotations_file, PhysPageNumber num_page_frames, PageShift shift);

        void allocate() override;

    private:
        std::unordered_map<VirtPageNumber, PageTableEntry> page_table;
        util::PriorityQueue<BeladyScore, VirtPageNumber> next_use_heap;
        platform::MappedFile<ProgramFileHeader> virt_prog;
        platform::MappedFile<Annotation> annotations;
        PageShift page_shift;
    };
}

#endif
