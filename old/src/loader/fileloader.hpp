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

#ifndef MAGE_LOADER_FILELOADER_HPP_
#define MAGE_LOADER_FILELOADER_HPP_

#include <cstdint>

#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "loader.hpp"

namespace mage {
    /* Defined in scheduler.hpp */
    class Scheduler;
}

namespace mage::loader {
    class FileLoader : public GatePageLoader {
        struct FreeListNode {
            FreeListNode* next;
        };

        std::uint8_t* pages;
        std::uint64_t page_size;
        std::uint64_t num_resident_pages;
        std::ifstream input;
        FreeListNode* free_list;
        std::mutex free_list_mutex;
        std::condition_variable free_list_cond;
        bool done;

        std::thread* worker_thread;

    public:
        FileLoader(std::string path, std::uint64_t page_size, std::int64_t num_resident_pages);
        ~FileLoader();
        void gate_page_executed(GatePageID id, GatePage* page);
        void start_loading(Scheduler* s);

    private:
        void worker(Scheduler* s);
    };
}

#endif
