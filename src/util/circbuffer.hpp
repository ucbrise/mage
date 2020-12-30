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

#ifndef MAGE_UTIL_CIRCBUFFER_HPP_
#define MAGE_UTIL_CIRCBUFFER_HPP_

#include <cstddef>
#include <algorithm>
#include "platform/memory.hpp"

namespace mage::util {
    template <typename T>
    class CircularBuffer {
    public:
        CircularBuffer(std::size_t buffer_capacity) : data(sizeof(T) * buffer_capacity, false), read_index(0), write_index(0), capacity(buffer_capacity), length(0) {
        }

        std::size_t get_space_occupied() const {
            return this->length;
        }

        void read_unchecked(T* elements, std::size_t count) {
            const T* buffer = this->data.mapping();
            std::size_t until_end = this->capacity - this->read_index;
            if (until_end <= count) {
                std::copy(&buffer[this->read_index], &buffer[this->capacity], &elements[0]);
                std::copy(&buffer[0], &buffer[count - until_end], &elements[until_end]);
                this->read_index = count - until_end;
            } else {
                std::copy(&buffer[this->read_index], &buffer[this->read_index + count], &elements[0]);
                this->read_index += count;
            }
            this->length -= count;
        }

        bool read_checked(T* elements, std::size_t count) {
            if (count > this->get_space_occupied()) {
                return false;
            }
            this->read_unchecked(elements, count);
            return true;
        }

        std::size_t read_nonblock(T* elements, std::size_t count) {
            count = std::min(count, this->get_space_occupied());
            this->read_unchecked(elements, count);
            return count;
        }

        std::size_t get_space_unoccupied() const {
            return this->capacity - this->length;
        }

        void write_unchecked(const T* elements, std::size_t count) {
            T* buffer = this->data.mapping();
            std::size_t until_end = this->capacity - this->write_index;
            if (until_end <= count) {
                std::copy(&elements[0], &elements[until_end], &buffer[this->write_index]);
                std::copy(&elements[until_end], &elements[count], &buffer[0]);
                this->write_index = count - until_end;
            } else {
                std::copy(&elements[0], &elements[count], &buffer[this->write_index]);
                this->write_index += count;
            }
            this->length += count;
        }

        bool write_checked(const T* elements, std::size_t count) {
            if (this->get_space_unoccupied() < count) {
                return false;
            }
            this->write_unchecked(elements, count);
            return true;
        }

        std::size_t write_nonblock(const T* elements, std::size_t count) {
            count = std::min(count, this->get_space_unoccupied());
            this->write_unchecked(elements, count);
            return count;
        }

        T& start_write_unchecked() {
            T* buffer = this->data.mapping();
            return buffer[this->write_index];
        }

        bool start_write_checked(T** target) {
            if (this->get_space_unoccupied() == 0) {
                return false;
            }
            *target = &this->start_write_single_unchecked();
            return true;
        }

        void finish_write(std::size_t amount = 1) {
            this->write_index += amount;
            if (this->capacity <= this->write_index) {
                this->write_index -= this->capacity;
            }
            this->length += amount;
        }

        T& start_read_unchecked() {
            T* buffer = this->data.mapping();
            return buffer[this->read_index];
        }

        bool start_read_checked(T** target) {
            if (this->get_space_occupied() == 0) {
                return false;
            }
            *target = &this->start_read_single_unchecked();
            return true;
        }

        void finish_read(std::size_t amount = 1) {
            this->read_index += amount;
            if (this->capacity <= this->read_index) {
                this->read_index -= this->capacity;
            }
            this->length -= amount;
        }

    private:
        platform::MappedFile<T> data;
        std::size_t read_index;
        std::size_t write_index;
        std::size_t capacity;
        std::size_t length;
    };
}

#endif
