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
#include <memory>
#include <string>
#include <unordered_map>
#include "instruction.hpp"
#include "engine/cluster.hpp"
#include "platform/filesystem.hpp"
#include "platform/memory.hpp"
#include "util/resourceset.hpp"
#include "util/stats.hpp"

namespace mage::engine {
    template <typename ProtEngine>
    class Engine {
        static const constexpr int aio_max_events = 2048;
        static const constexpr int aio_process_batch_size = 64;

    public:
        Engine(std::shared_ptr<ClusterNetwork>& network, ProtEngine& prot) : protocol(prot),
            memory(nullptr), memory_size(0), swap_in("SWAP-IN (ns)", true),
            swap_out("SWAP-OUT (ns)", true), swap_blocked("SWAP_BLOCKED (ns)", true),
            cluster(network), aio_ctx(0) {
        }

        virtual ~Engine();

        void init(const util::ResourceSet::Worker& worker, PageShift shift, std::uint64_t num_pages, std::uint64_t swap_pages, std::uint32_t concurrent_swaps);

        std::size_t execute_instruction(const PackedPhysInstruction& phys);
        void wait_for_finish_swap(PhysPageNumber ppn);

        void execute_issue_swap_in(const PackedPhysInstruction& phys);
        void execute_issue_swap_out(const PackedPhysInstruction& phys);
        void execute_finish_swap_in(const PackedPhysInstruction& phys);
        void execute_finish_swap_out(const PackedPhysInstruction& phys);
        void execute_copy_swap(const PackedPhysInstruction& phys);
        void execute_network_post_receive(const PackedPhysInstruction& phys);
        void execute_network_finish_receive(const PackedPhysInstruction& phys);
        void execute_network_buffer_send(const PackedPhysInstruction& phys);
        void execute_network_finish_send(const PackedPhysInstruction& phys);
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
        /*
         * There is nothing protocol-specific about this method --- perhaps
         * move to a non-template superclass?
         */
        MessageChannel& contact_worker_checked(WorkerID worker_id);

        ProtEngine& protocol;
        typename ProtEngine::Wire* memory;
        PageShift page_shift;
        std::size_t memory_size;
        int swapfd;

        std::shared_ptr<ClusterNetwork> cluster;

        io_context_t aio_ctx;
        std::unordered_map<PhysPageNumber, struct iocb> in_flight_swaps;
    };
}

#endif
