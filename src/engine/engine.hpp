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
 * @file engine/engine.hpp
 * @brief Common functionality used by all MAGE engines.
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
#include "util/progress.hpp"
#include "util/stats.hpp"

namespace mage::engine {
    /**
     * @brief Base Engine class implementing functionality common to all
     * engines. All engines are expected to inherit from this one.
     *
     * The base Engine works in byte addresses, not wire addresses. If a
     * specialized engine is working in wire addresses, the addresses should be
     * converted to byte addresses before passing them to the base engine. The
     * provided page size is assumed to be in bytes; that way, page numbers can
     * be passed to the base engine unmodified.
     *
     * @param network This worker's network endpoint for intra-party
     * communication.
     */
    class Engine {
        static const constexpr int aio_max_events = 2048;
        static const constexpr int aio_process_batch_size = 64;

    public:
        /**
         * @brief Creates an Engine instance that uses the provided
         * ClusterNetwork to communicate with other workers in the same party.
         *
         * Before using this Engine instance, one should call the init()
         * member function to properly initialize it. It is recommended that
         * subclasses call Engine::init() in their constructors.
         */
        Engine(const std::shared_ptr<ClusterNetwork>& network) : memory(nullptr),
            memory_size(0), swapfd(-1), swap_in("SWAP-IN (ns)", true),
            swap_out("SWAP-OUT (ns)", true), swap_blocked("SWAP-BLOCKED (ns)", true),
            cluster(network), aio_ctx(0), progress_bar("Execution", 1024) {
        }

        virtual ~Engine();

    protected:
        /**
         * @brief Initializes this Engine instance (necessary before it can
         * perform useful work).
         *
         * @param storage_file The file path of the file or device used for
         * swap space.
         * @param page_size_in_byte The size of a page (unit of transfer
         * between memory and storage) in bytes.
         * @param num_pages The total number of pages resident in memory.
         * @param swap_pages The total size of the swap file or swap device,
         * in pages.
         * @param concurrent_swaps The maximum number of outstanding transfers
         * between memory and storage at any one point in time.
         */
        void init(const std::string& storage_file, PageSize page_size_in_bytes, std::uint64_t num_pages, std::uint64_t swap_pages, std::uint32_t concurrent_swaps);

        /**
         * @brief Initiates the transfer of a page from storage to memory.
         *
         * @param spn The page number identifying the location in the storage
         * file or device from which to read the page.
         * @param ppn The page number identifying the loction in memory into
         * which to write the page.
         */
        void issue_swap_in(StoragePageNumber spn, PhysPageNumber ppn);

        /**
         * @brief Initiates the transfer of a page from memory to storage.
         *
         * @param ppn The page number identifying the location in memory from
         * which to read the page.
         * @param spn The page number identifying the loction in the storage
         * file or device into which to write the page.
         */
        void issue_swap_out(PhysPageNumber ppn, StoragePageNumber spn);

        /**
         * @brief Block until the specified transfer of a page between memory
         * and storage is complete.
         *
         * @param ppn The page number identifying the location in memory that
         * the transfer operates on.
         */
        void wait_for_finish_swap(PhysPageNumber ppn);

        /**
         * @brief Copies a page from one location in memory to another.
         *
         * @param from The page number identifying th elocation in memory from
         * which to read the page.
         * @param to The page number identifying the loction in memory into
         * which to write the page.
         */
        void copy_page(PhysPageNumber from, PhysPageNumber to);

        /**
         * @brief Initiates asynchronous receipt of data into the specified
         * memory.
         *
         * @tparam Wire Type indicating the unit by which the amount of data
         * is specified.
         * @param who The ID of the worker from which data should be received.
         * @param into A pointer to the memory into which the data should be
         * received.
         * @param num_wires The amount of data to read, in units of @p Wire.
         */
        template <typename Wire = std::uint8_t>
        void network_post_receive(WorkerID who, Wire* into, std::size_t num_wires) {
            MessageChannel& c = this->contact_worker_checked(who);
            AsyncRead& ar = c.start_post_read();
            ar.into = into;
            ar.length = num_wires * sizeof(Wire);
            c.finish_post_read();
        }

        /**
         * @brief Blocks until all pending receive operations (initiated via
         * network_post_receive()) from the specified worker are complete.
         *
         * @param who The ID of the specified worker.
         */
        void network_finish_receive(WorkerID who) {
            MessageChannel& c = this->contact_worker_checked(who);
            c.wait_until_reads_finished();
        }

        /**
         * @brief Initiates sending data to the specified worker, placing them
         * into an outgoing buffer.
         *
         * @tparam Wire Type indicating the unit by which the amount of data
         * is specified.
         * @param who The ID of the worker to which data should be sent.
         * @param into A pointer to the data to be sent.
         * @param num_wires The amount of data to send, in units of @p Wire.
         */
        template <typename Wire = std::uint8_t>
        void network_buffer_send(WorkerID who, Wire* from, std::size_t num_wires) {
            MessageChannel& c = this->contact_worker_checked(who);
            // typename ProtEngine::Wire* buffer = c.write<typename ProtEngine::Wire>(num_wires);
            Wire* buffer = c.write<Wire>(num_wires);
            std::copy(from, from + num_wires, buffer);
        }

        /**
         * @brief Blocks until all pending data destined for the specified
         * worker are sent.
         *
         * @param who The ID of the specified worker.
         */
        void network_finish_send(WorkerID who) {
            this->contact_worker_checked(who).flush();
        }

        /**
         * @brief Prints statistics on the performance of transferring data
         * between memory and storage to standard output, in human-readable
         * form.
         */
        void print_stats() {
            std::cout << this->swap_in << std::endl;
            std::cout << this->swap_out << std::endl;
            std::cout << this->swap_blocked << std::endl;
        }

        /**
         * @brief Obtains a timestamp for the current time and sets it as the
         * current timer.
         */
        void start_timer() {
            this->current_timer_value = std::chrono::steady_clock::now();
        }

        /**
         * @brief Writes to standard output, in human-readable form, the time
         * elapased between the previous call to Engine::start_timer() and the
         * current time.
         */
        void stop_timer() {
            auto end = std::chrono::steady_clock::now();
            std::cout << "Timer: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - this->current_timer_value).count() << " ns" << std::endl;
        }

        /**
         * @brief Returns a pointer to the memory allocated for protocol
         * execution.
         */
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

    protected:
        util::ProgressBar progress_bar;
    };
}

#endif
