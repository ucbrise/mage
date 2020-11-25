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

#include "engine/engine.hpp"
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <libaio.h>
#include <chrono>
#include <string>
#include "addr.hpp"
#include "instruction.hpp"
#include "opcode.hpp"
#include "platform/filesystem.hpp"
#include "util/stats.hpp"

namespace mage::engine {
    void Engine::init(const std::string& storage_file, PageShift shift, std::uint64_t num_pages, std::uint64_t swap_pages, std::uint32_t concurrent_swaps) {
        assert(this->memory == nullptr);

        if (io_setup(concurrent_swaps, &this->aio_ctx) != 0) {
            std::perror("io_setup");
            std::abort();
        }

        this->memory_size = pg_addr(num_pages, shift);
        this->memory = platform::allocate_resident_memory<std::uint8_t>(this->memory_size);
        std::uint64_t required_size = pg_addr(swap_pages, shift);
        if (storage_file.rfind("/dev/", 0) != std::string::npos) {
            std::uint64_t length;
            this->swapfd = platform::open_file(storage_file.c_str(), &length, true);
            if (length < required_size) {
                std::cerr << "Disk too small: size is " << length << " B, requires " << required_size << " B" << std::endl;
                std::abort();
            }
        } else {
            this->swapfd = platform::create_file(storage_file.c_str(), required_size, true, true);
        }
        this->page_shift = shift;
    }

    Engine::~Engine()  {
        if (this->aio_ctx != 0 && io_destroy(this->aio_ctx) != 0) {
            std::perror("io_destroy");
            std::abort();
        }
        if (this->memory_size != 0) {
            platform::deallocate_resident_memory(this->memory, this->memory_size);
        }
        if (this->swapfd != -1) {
            platform::close_file(this->swapfd);
        }
    }

    void Engine::issue_swap_in(StoragePageNumber spn, PhysPageNumber ppn) {
        /* These are byte addresses (not wire addresses). */
        StorageAddr saddr = pg_addr(spn, this->page_shift);
        PhysAddr paddr = pg_addr(ppn, this->page_shift);

        auto start = std::chrono::steady_clock::now();
        assert(this->in_flight_swaps.find(ppn) == this->in_flight_swaps.end());
        struct iocb& op = this->in_flight_swaps[ppn];
        struct iocb* op_ptr = &op;
        // io_prep_pread(op_ptr, this->swapfd, &this->memory[paddr], pg_size(this->page_shift) * sizeof(typename ProtEngine::Wire), saddr * sizeof(typename ProtEngine::Wire));
        io_prep_pread(op_ptr, this->swapfd, &this->memory[paddr], pg_size(this->page_shift), saddr);
        op_ptr->data = &this->memory[paddr];
        int rv = io_submit(this->aio_ctx, 1, &op_ptr);
        if (rv != 1) {
            std::cerr << "io_submit: " << std::strerror(-rv) << std::endl;
            std::abort();
        }
        // platform::read_from_file_at(this->swapfd, &this->memory[paddr], pg_size(this->page_shift) * sizeof(typename ProtEngine::Wire), saddr * sizeof(typename ProtEngine::Wire));
        auto end = std::chrono::steady_clock::now();
        this->swap_in.event(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    void Engine::issue_swap_out(PhysPageNumber ppn, StoragePageNumber spn) {
        /* These are byte addresses (not wire addresses). */
        PhysAddr paddr = pg_addr(ppn, this->page_shift);
        StorageAddr saddr = pg_addr(spn, this->page_shift);

        auto start = std::chrono::steady_clock::now();
        assert(this->in_flight_swaps.find(ppn) == this->in_flight_swaps.end());
        struct iocb& op = this->in_flight_swaps[ppn];
        struct iocb* op_ptr = &op;
        // io_prep_pwrite(op_ptr, this->swapfd, &this->memory[paddr], pg_size(this->page_shift) * sizeof(typename ProtEngine::Wire), saddr * sizeof(typename ProtEngine::Wire));
        io_prep_pwrite(op_ptr, this->swapfd, &this->memory[paddr], pg_size(this->page_shift), saddr);
        op_ptr->data = &this->memory[paddr];
        int rv = io_submit(this->aio_ctx, 1, &op_ptr);
        if (rv != 1) {
            std::cerr << "io_submit: " << std::strerror(-rv) << std::endl;
            std::abort();
        }
        //platform::write_to_file_at(this->swapfd, &this->memory[paddr], pg_size(this->page_shift) * sizeof(typename ProtEngine::Wire), saddr * sizeof(typename ProtEngine::Wire));
        auto end = std::chrono::steady_clock::now();
        this->swap_out.event(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    void Engine::wait_for_finish_swap(PhysPageNumber ppn) {
        auto iter = this->in_flight_swaps.find(ppn);
        if (iter == this->in_flight_swaps.end()) {
            return;
        }

        auto start = std::chrono::steady_clock::now();

        bool found = false;
        do {
            struct io_event events[aio_process_batch_size];
            int rv = io_getevents(this->aio_ctx, 1, aio_process_batch_size, events, nullptr);
            if (rv < 1) {
                std::cerr << "io_getevents: " << std::strerror(-rv) << std::endl;
                std::abort();
            }
            for (int i = 0; i != rv; i++) {
                struct io_event& event = events[i];
                std::uint8_t* page_start = reinterpret_cast<std::uint8_t*>(event.data);
                assert(page_start != nullptr);
                PhysPageNumber found_ppn = pg_num(page_start - this->memory, this->page_shift);

                iter = this->in_flight_swaps.find(found_ppn);
                assert(iter != this->in_flight_swaps.end());
                assert(event.obj == &iter->second);
                this->in_flight_swaps.erase(iter);
                if (event.res < 0) {
                    std::cerr << "Swap failed" << std::endl;
                    std::abort();
                }

                found = (found || (found_ppn == ppn));
            }
        } while (!found);

        auto end = std::chrono::steady_clock::now();
        this->swap_blocked.event(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    void Engine::copy_page(PhysPageNumber from, PhysPageNumber to) {
        PhysAddr to_paddr = pg_addr(to, this->page_shift);
        PhysAddr from_paddr = pg_addr(from, this->page_shift);

        std::copy(&this->memory[from_paddr], &this->memory[from_paddr + pg_size(this->page_shift)], &this->memory[to_paddr]);
    }

    MessageChannel& Engine::contact_worker_checked(WorkerID worker_id) {
        MessageChannel* channel = this->cluster->contact_worker(worker_id);
        if (channel == nullptr) {
            std::cerr << "Attempted to contact worker " << worker_id << std::endl;
            std::abort();
        }
        return *channel;
    }
}
