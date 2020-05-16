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

#ifndef MAGE_ENGINE_ENGINE_HPP_
#define MAGE_ENGINE_ENGINE_HPP_

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include "instruction.hpp"
#include "platform/filesystem.hpp"
#include "platform/memory.hpp"
#include "util/stats.hpp"

namespace mage::engine {
    template <typename Protocol>
    class Engine {
    public:
        Engine(Protocol& prot) : protocol(prot), memory(nullptr), memory_size(0), swap_in("SWAP-IN (us)", true), swap_out("SWAP-OUT (us)", true) {
        }

        void init(PageShift shift, std::uint64_t num_pages, std::uint64_t swap_pages, std::string swapfile) {
            assert(this->memory == nullptr);
            this->memory_size = pg_addr(num_pages, shift) * sizeof(typename Protocol::Wire);
            this->memory = platform::allocate_resident_memory<typename Protocol::Wire>(this->memory_size);
            std::uint64_t required_size = pg_addr(swap_pages, shift) * sizeof(typename Protocol::Wire);
            if (swapfile.starts_with("/dev/")) {
                std::uint64_t length;
                this->swapfd = platform::open_file(swapfile.c_str(), &length, true);
                if (length < required_size) {
                    std::cerr << "Disk too small: size is " << length << " B, requires " << required_size << " B" << std::endl;
                    std::abort();
                }
            } else {
                this->swapfd = platform::create_file(swapfile.c_str(), required_size, true, true);
            }
            this->page_shift = shift;
        }

        virtual ~Engine() {
            platform::deallocate_resident_memory(this->memory, this->memory_size);
            platform::close_file(this->swapfd);
        }

        std::size_t execute_instruction(const PackedPhysInstruction& phys);

        void execute_swap_in(const PackedPhysInstruction& phys);
        void execute_swap_out(const PackedPhysInstruction& phys);
        void execute_public_constant(const PackedPhysInstruction& phys);
        void execute_int_add(const PackedPhysInstruction& phys);
        void execute_int_increment(const PackedPhysInstruction& phys);
        void execute_int_sub(const PackedPhysInstruction& phys);
        void execute_int_decrement(const PackedPhysInstruction& phys);
        void execute_int_less(const PackedPhysInstruction& phys);
        void execute_equal(const PackedPhysInstruction& phys);
        void execute_is_zero(const PackedPhysInstruction& phys);
        void execute_non_zero(const PackedPhysInstruction& phys);
        void execute_bit_not(const PackedPhysInstruction& phys);
        void execute_bit_and(const PackedPhysInstruction& phys);
        void execute_bit_or(const PackedPhysInstruction& phys);
        void execute_bit_xor(const PackedPhysInstruction& phys);
        void execute_value_select(const PackedPhysInstruction& phys);

        util::StreamStats swap_in;
        util::StreamStats swap_out;

    private:
        Protocol& protocol;
        typename Protocol::Wire* memory;
        PageShift page_shift;
        std::size_t memory_size;
        int swapfd;
    };
}

#endif
