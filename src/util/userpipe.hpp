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

#ifndef MAGE_UTIL_USERPIPE_HPP_
#define MAGE_UTIL_USERPIPE_HPP_

#include <cstddef>
#include <condition_variable>
#include <mutex>
#include "util/circbuffer.hpp"

namespace mage::util {
    template <typename T>
    class UserPipe : private CircularBuffer<T> {
    public:
        UserPipe(std::size_t capacity_shift) : CircularBuffer<T>(capacity_shift), closed(false) {
        }

        /*
         * Closing a pipe disallows writes to the pipe. Reads can still get
         * what was previously written to the pipe, but they will no longer
         * block waiting for more data.
         */
        void close() {
            std::lock_guard<std::mutex> lock(this->mutex);
            this->closed = true;
            this->added.notify_all();
            this->removed.notify_all();
        }

        std::size_t read_contiguous(T* elements, std::size_t count) {
            std::unique_lock<std::mutex> lock(this->mutex);
            while (this->get_space_occupied() < count && !this->closed) {
                this->added.wait(lock);
            }
            std::size_t to_read = std::min(this->get_space_occupcied(), count);
            this->read_unchecked(elements, to_read);
            this->removed.notify_all();
            return to_read;
        }

        bool write_contiguous(const T* elements, std::size_t count) {
            std::unique_lock<std::mutex> lock(this->mutex);
            while (this->get_space_unoccupied() < count && !this->closed) {
                this->removed.wait(lock);
            }
            if (this->closed) {
                return false;
            }
            this->write_unchecked(elements, count);
            this->added.notify_all();
            return true;
        }

        /*
         * In-place API potentially allows you to omit some copying, but is
         * harder to use. In particular, multiple readers can't concurrently
         * read in place; you'll need to provide external synchronization to
         * serialize that. Similarly, multiple writers can't concurrently write
         * in place. For the single-reader/single-writer cases, however, this
         * should not be too difficult to use.
         */

        T* start_read_single_in_place() {
            std::unique_lock<std::mutex> lock(this->mutex);
            while (this->get_space_occupied() == 0 && !this->closed) {
                this->added.wait(lock);
            }
            if (this->get_space_occupied() == 0) {
                return nullptr;
            }
            return &(this->start_read_single_unchecked());
        }

        void finish_read_single_in_place() {
            std::lock_guard<std::mutex> lock(this->mutex);
            assert(this->get_space_occupied() != 0);
            this->finish_read_single();
            this->removed.notify_all();
        }

        T* start_write_single_in_place() {
            std::unique_lock<std::mutex> lock(this->mutex);
            while (this->get_space_unoccupied() == 0 && !this->closed) {
                this->removed.wait(lock);
            }
            if (this->closed) {
                return nullptr;
            }
            return &(this->start_write_single_unchecked());
        }

        void finish_write_single_in_place() {
            std::lock_guard<std::mutex> lock(this->mutex);
            assert(this->get_space_unoccupied() != 0);
            this->finish_write_single();
            this->added.notify_all();
        }

        /* Interface for synchronization based on internal events. */

        std::unique_lock<std::mutex> lock() {
            return std::unique_lock<std::mutex>(&mutex);
        }

        void wait_for_addition(std::unique_lock<std::mutex>& lock) {
            this->added.wait(lock);
        }

        void wait_for_removal(std::unique_lock<std::mutex>& lock) {
            this->removed.wait(lock);
        }

    private:
        std::mutex mutex;
        std::condition_variable added;
        std::condition_variable removed;
        bool closed;
    };
}

#endif
