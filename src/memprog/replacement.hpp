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
#include "memprog/addr.hpp"
#include "memprog/annotation.hpp"
#include "memprog/instruction.hpp"
#include "memprog/opcode.hpp"
#include "memprog/programfile.hpp"
#include "platform/memory.hpp"
#include "util/prioqueue.hpp"

namespace mage::memprog {
    class Allocator {
    public:
        Allocator(std::string output_file);
        virtual ~Allocator();

        virtual void allocate() = 0;

        std::uint64_t get_num_swapouts() const;
        std::uint64_t get_num_swapins() const;

    protected:
        void emit_instruction(const PackedPhysInstruction& phys, std::size_t len) {
            this->phys_prog.append_instruction(phys, len);
        }
        void emit_instruction(const PackedPhysInstruction& phys) {
            this->emit_instruction(phys, phys.size());
        }
        void emit_swapout(PhysPageNumber primary, VirtPageNumber secondary);
        void emit_swapin(VirtPageNumber secondary, PhysPageNumber primary);

    private:
        /* Keeps track of the number of swaps performed in the allocation. */
        std::uint64_t num_swapouts;
        std::uint64_t num_swapins;
        PhysProgramFileWriter phys_prog;
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

    class BeladyAllocator : public Allocator {
    public:
        BeladyAllocator(std::string output_file, std::string virtual_program_file, std::string annotations_file, PhysPageNumber num_physical_pages, PageShift shift);

        void allocate() override;

    private:
        std::vector<PhysPageNumber> free_list;
        std::unordered_map<VirtPageNumber, PhysPageNumber> page_table;
        util::PriorityQueue<BeladyScore, VirtPageNumber> next_use_heap;
        platform::MappedFile<ProgramFileHeader> virt_prog;
        platform::MappedFile<Annotation> annotations;
        PageShift page_shift;
    };
}

#endif
