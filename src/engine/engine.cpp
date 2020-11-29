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
#include <filesystem>
#include <string>
#include "addr.hpp"
#include "instruction.hpp"
#include "opcode.hpp"
#include "platform/filesystem.hpp"
#include "util/stats.hpp"

namespace mage::engine {
    void Engine::init(const std::string& storage_file, PageSize page_size_in_bytes, std::uint64_t num_pages, std::uint64_t swap_pages, std::uint32_t concurrent_swaps) {
        assert(this->memory == nullptr);
        auto start = std::chrono::steady_clock::now();

        if (io_setup(concurrent_swaps, &this->aio_ctx) != 0) {
            std::perror("io_setup");
            std::abort();
        }

        this->memory_size = num_pages * page_size_in_bytes;
        auto mem_start = std::chrono::steady_clock::now();
        this->memory = platform::allocate_resident_memory<std::uint8_t>(this->memory_size);
        auto mem_end = std::chrono::steady_clock::now();
        std::uint64_t required_size = swap_pages * page_size_in_bytes;
        if (storage_file.rfind("/dev/", 0) != std::string::npos) {
            std::uint64_t length;
            this->swapfd = platform::open_file(storage_file.c_str(), &length, true);
            if (length < required_size) {
                std::cerr << "Disk too small: size is " << length << " B, requires " << required_size << " B" << std::endl;
                std::abort();
            }
        } else {
            bool create = false;
            if (std::filesystem::exists(storage_file)) {
                std::uint64_t length;
                this->swapfd = platform::open_file(storage_file.c_str(), &length, true);
                if (length < required_size) {
                    platform::close_file(this->swapfd);
                    create = true;
                }
            } else {
                create = true;
            }
            if (create) {
                this->swapfd = platform::create_file(storage_file.c_str(), required_size, true, true);
            }
        }
        this->page_size_bytes = page_size_in_bytes;

        auto end = std::chrono::steady_clock::now();
        std::cout << "Memory alloc time: " << std::chrono::duration_cast<std::chrono::milliseconds>(mem_end - mem_start).count() << " ms" << std::endl;
        std::cout << "Total init time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
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
        StorageAddr saddr = spn * this->page_size_bytes;
        PhysAddr paddr = ppn * this->page_size_bytes;

        auto start = std::chrono::steady_clock::now();
        assert(this->in_flight_swaps.find(paddr) == this->in_flight_swaps.end());
        struct iocb& op = this->in_flight_swaps[paddr];
        struct iocb* op_ptr = &op;
        // io_prep_pread(op_ptr, this->swapfd, &this->memory[paddr], pg_size(this->page_shift) * sizeof(typename ProtEngine::Wire), saddr * sizeof(typename ProtEngine::Wire));
        io_prep_pread(op_ptr, this->swapfd, &this->memory[paddr], this->page_size_bytes, saddr);
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
        PhysAddr paddr = ppn * this->page_size_bytes;
        StorageAddr saddr = spn * this->page_size_bytes;

        auto start = std::chrono::steady_clock::now();
        assert(this->in_flight_swaps.find(paddr) == this->in_flight_swaps.end());
        struct iocb& op = this->in_flight_swaps[paddr];
        struct iocb* op_ptr = &op;
        // io_prep_pwrite(op_ptr, this->swapfd, &this->memory[paddr], pg_size(this->page_shift) * sizeof(typename ProtEngine::Wire), saddr * sizeof(typename ProtEngine::Wire));
        io_prep_pwrite(op_ptr, this->swapfd, &this->memory[paddr], this->page_size_bytes, saddr);
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
        PhysAddr paddr = ppn * this->page_size_bytes;

        auto iter = this->in_flight_swaps.find(paddr);
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
                PhysAddr found_paddr = page_start - this->memory;

                iter = this->in_flight_swaps.find(found_paddr);
                assert(iter != this->in_flight_swaps.end());
                assert(event.obj == &iter->second);
                this->in_flight_swaps.erase(iter);
                if (event.res < 0) {
                    std::cerr << "Swap failed" << std::endl;
                    std::abort();
                }

                found = (found || (found_paddr == paddr));
            }
        } while (!found);

        auto end = std::chrono::steady_clock::now();
        this->swap_blocked.event(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    void Engine::copy_page(PhysPageNumber from, PhysPageNumber to) {
        PhysAddr to_paddr = to * this->page_size_bytes;
        PhysAddr from_paddr = from * this->page_size_bytes;

        std::copy(&this->memory[from_paddr], &this->memory[from_paddr + this->page_size_bytes], &this->memory[to_paddr]);
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
