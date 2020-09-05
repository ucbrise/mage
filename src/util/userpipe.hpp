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
        UserPipe(std::size_t capacity_shift) : CircularBuffer<T>(capacity_shift) {
        }

        void read_contiguous(T* elements, std::size_t count) {
            std::unique_lock<std::mutex> lock(this->mutex);
            while (this->get_space_occupied() < count) {
                this->added.wait(lock);
            }
            this->read_unchecked(elements, count);
            this->removed.notify_all();
        }

        void write_contiguous(const T* elements, std::size_t count) {
            std::unique_lock<std::mutex> lock(this->mutex);
            while (this->get_space_unoccupied() < count) {
                this->removed.wait(lock);
            }
            this->write_unchecked(elements, count);
            this->added.notify_all();
        }

    private:
        std::mutex mutex;
        std::condition_variable added;
        std::condition_variable removed;
    };
}

#endif
