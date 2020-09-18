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
#include <cstdio>
#include <cstdlib>
#include <libaio.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include "instruction.hpp"
#include "platform/filesystem.hpp"
#include "platform/memory.hpp"
#include "util/stats.hpp"

namespace mage::engine {
    template <typename ProtEngine>
    class Engine {
        static const constexpr int aio_max_events = 2048;
        static const constexpr int aio_process_batch_size = 64;
    public:
        Engine(ProtEngine& prot) : protocol(prot), memory(nullptr), memory_size(0), swap_in("SWAP-IN (ns)", true), swap_out("SWAP-OUT (ns)", true), swap_blocked("SWAP_BLOCKED (ns)", true), aio_ctx(0) {
        }

        void init(PageShift shift, std::uint64_t num_pages, std::uint64_t swap_pages, std::uint32_t concurrent_swaps, std::string swapfile) {
            assert(this->memory == nullptr);

            if (io_setup(concurrent_swaps, &this->aio_ctx) != 0) {
                std::perror("io_setup");
                std::abort();
            }

            this->memory_size = pg_addr(num_pages, shift) * sizeof(typename ProtEngine::Wire);
            this->memory = platform::allocate_resident_memory<typename ProtEngine::Wire>(this->memory_size);
            std::uint64_t required_size = pg_addr(swap_pages, shift) * sizeof(typename ProtEngine::Wire);
            if (swapfile.rfind("/dev/", 0) != std::string::npos) {
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
            if (io_destroy(this->aio_ctx) != 0) {
                std::perror("io_destroy");
                std::abort();
            }
            platform::deallocate_resident_memory(this->memory, this->memory_size);
            platform::close_file(this->swapfd);
        }

        std::size_t execute_instruction(const PackedPhysInstruction& phys);
        void wait_for_finish_swap(PhysPageNumber ppn);

        void execute_issue_swap_in(const PackedPhysInstruction& phys);
        void execute_issue_swap_out(const PackedPhysInstruction& phys);
        void execute_finish_swap_in(const PackedPhysInstruction& phys);
        void execute_finish_swap_out(const PackedPhysInstruction& phys);
        void execute_copy_swap(const PackedPhysInstruction& phys);
        void execute_public_constant(const PackedPhysInstruction& phys);
        void execute_copy(const PackedPhysInstruction& phys);
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
        util::StreamStats swap_blocked;

    private:
        ProtEngine& protocol;
        typename ProtEngine::Wire* memory;
        PageShift page_shift;
        std::size_t memory_size;
        int swapfd;

        io_context_t aio_ctx;
        std::unordered_map<PhysPageNumber, struct iocb> in_flight_swaps;
    };
}

#endif
