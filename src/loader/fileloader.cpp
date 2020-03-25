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

#include "fileloader.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

#include "../gate.hpp"
#include "../scheduler.hpp"
#include "../platform/memory.hpp"

namespace mage::loader {
    FileLoader::FileLoader(std::string path, std::uint64_t page_size, std::int64_t num_resident_pages) {
        /* Allocate memory for gate pages according to arguments */
        this->page_size = page_size;
        this->num_resident_pages = num_resident_pages;
        void* allocated = platform::allocate_resident_memory(page_size * num_resident_pages);
        this->pages = reinterpret_cast<std::uint8_t*>(allocated);
        this->done = false;
        this->worker_thread = nullptr;

        /* Initialize free list */
        this->free_list = reinterpret_cast<FreeListNode*>(&this->pages[page_size * (num_resident_pages - 1)]);
        for (std::int64_t i = num_resident_pages - 2; i != -1; i++) {
            FreeListNode* node = reinterpret_cast<FreeListNode*>(&this->pages[page_size * i]);
            node->next = this->free_list;
            this->free_list = node;
        }

        /* Disable buffering (see https://stackoverflow.com/questions/16605233/how-to-disable-buffering-on-a-stream) */
        this->input.rdbuf()->pubsetbuf(0, 0);

        /* Open file */
        this->input.open(path, std::ios::in | std::ios::binary);
        if (!this->input.is_open()) {
            std::cout << "Could not open file " << path << std::endl;
            std::abort();
        }
    }

    FileLoader::~FileLoader() {
        platform::deallocate_resident_memory(this->pages, this->page_size * this->num_resident_pages);
        delete this->worker_thread;
    }

    void FileLoader::gate_page_executed(GatePageID id, GatePage* page) {
        std::unique_lock<std::mutex> lock(this->free_list_mutex);

        /* Add page to free list */
        FreeListNode* node = reinterpret_cast<FreeListNode*>(page);
        node->next = this->free_list;
        this->free_list = node;

        /* Signal thread */
        this->free_list_cond.notify_one();
    }

    void FileLoader::start_loading(Scheduler* s) {
        /* Start worker thread */
        this->worker_thread = new std::thread(&FileLoader::worker, this, s);
        this->worker_thread->detach();
    }

    void FileLoader::worker(Scheduler* s) {
        GatePageID gid = 0;
        std::unique_lock<std::mutex> lock(this->free_list_mutex);
        while (this->input.good()) {
            while (this->free_list == nullptr) {
                this->free_list_cond.wait(lock);
            }

            FreeListNode* first = this->free_list;
            this->free_list = first->next;

            char* buffer = reinterpret_cast<char*>(first);
            this->input.read(buffer, this->page_size);

            gid++;
            GatePage* page = reinterpret_cast<GatePage*>(buffer);
            s->gate_page_loaded(gid, page);
        }
        assert(!this->input.fail());
    }
}
