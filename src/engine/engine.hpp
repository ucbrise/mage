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
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include "instruction.hpp"
#include "engine/cluster.hpp"
#include "platform/filesystem.hpp"
#include "platform/memory.hpp"
#include "util/stats.hpp"

namespace mage::engine {
    /*
     * The base Engine works in byte addresses, not wire addresses. If a
     * specialized engine is working in wire addresses, the addresses should be
     * converted to byte addresses before passing them to the base engine. The
     * provided page size is assumed to be in bytes; that way, page numbers can
     * be passed to the base engine unmodified.
     */
    class Engine {
        static const constexpr int aio_max_events = 2048;
        static const constexpr int aio_process_batch_size = 64;

    public:
        Engine(const std::shared_ptr<ClusterNetwork>& network) : memory(nullptr),
            memory_size(0), swapfd(-1), swap_in("SWAP-IN (ns)", true),
            swap_out("SWAP-OUT (ns)", true), swap_blocked("SWAP-BLOCKED (ns)", true),
            cluster(network), aio_ctx(0) {
        }

        virtual ~Engine();

        void init(const std::string& storage_file, PageSize page_size_in_bytes, std::uint64_t num_pages, std::uint64_t swap_pages, std::uint32_t concurrent_swaps);

        void issue_swap_in(StoragePageNumber spn, PhysPageNumber ppn);
        void issue_swap_out(PhysPageNumber ppn, StoragePageNumber spn);
        void wait_for_finish_swap(PhysPageNumber ppn);
        void copy_page(PhysPageNumber from, PhysPageNumber to);

        template <typename Wire = std::uint8_t>
        void network_post_receive(WorkerID who, Wire* into, std::size_t num_wires) {
            MessageChannel& c = this->contact_worker_checked(who);
            AsyncRead& ar = c.start_post_read();
            ar.into = into;
            ar.length = num_wires * sizeof(Wire);
            c.finish_post_read();
        }

        void network_finish_receive(WorkerID who) {
            MessageChannel& c = this->contact_worker_checked(who);
            c.wait_until_reads_finished();
        }

        template <typename Wire = std::uint8_t>
        void network_buffer_send(WorkerID who, Wire* from, std::size_t num_wires) {
            MessageChannel& c = this->contact_worker_checked(who);
            // typename ProtEngine::Wire* buffer = c.write<typename ProtEngine::Wire>(num_wires);
            Wire* buffer = c.write<Wire>(num_wires);
            std::copy(from, from + num_wires, buffer);
        }

        void network_finish_send(WorkerID who) {
            this->contact_worker_checked(who).flush();
        }

        void print_stats() {
            std::cout << this->swap_in << std::endl;
            std::cout << this->swap_out << std::endl;
            std::cout << this->swap_blocked << std::endl;
        }

        void start_timer() {
            this->current_timer_value = std::chrono::steady_clock::now();
        }

        void stop_timer() {
            auto end = std::chrono::steady_clock::now();
            std::cout << "Timer: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - this->current_timer_value).count() << " ns" << std::endl;
        }

    protected:
        std::uint8_t* get_memory() const {
            return this->memory;
        }

    private:
        MessageChannel& contact_worker_checked(WorkerID worker_id);

        util::StreamStats swap_in;
        util::StreamStats swap_out;
        util::StreamStats swap_blocked;

        std::uint8_t* memory;
        PageSize page_size_bytes;
        std::size_t memory_size;
        int swapfd;

        std::chrono::steady_clock::time_point current_timer_value;

        std::shared_ptr<ClusterNetwork> cluster;

        io_context_t aio_ctx;
        std::unordered_map<PhysAddr, struct iocb> in_flight_swaps;
    };
}

#endif
