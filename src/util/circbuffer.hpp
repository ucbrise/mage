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
    /**
     * @brief A FIFO container whose capacity is fixed upon initialization.
     *
     * This is a data structure commonly known as a "circular buffer." Data can
     * be safely written (added) to a circular buffer if the number of stored
     * elements does not exceed the data structure's capacity after the
     * operation. Data can be safely read (removed) from a circular buffer if
     * it is not empty. A contiguous memory region is allocated up front
     * according to the capacity. Consecutively written elements are not
     * necessarily stored contiguously; elements "wrap around" to the beginning
     * of the region once the end is reached.
     *
     * The type @p T of elements in the circular buffer should not have a
     * custom destructor or copy constructor. The circular buffer treats the
     * elements as "raw memory." The destructor is not called when an object is
     * read, and the internal buffer is not initialized in any particular way
     * when a CircularBuffer instance is created.
     *
     * @tparam T The type of each element (datum) in the circular buffer.
     */
    template <typename T>
    class CircularBuffer {
    public:
        /**
         * @brief Creates a circular buffer.
         *
         * @param buffer_capacity Number of data elements that can be held.
         */
        CircularBuffer(std::size_t buffer_capacity) : data(sizeof(T) * buffer_capacity, false), read_index(0), write_index(0), capacity(buffer_capacity), length(0) {
        }

        /**
         * @brief Returns the number of data elements in the circular buffer.
         */
        std::size_t get_space_occupied() const {
            return this->length;
        }

        /**
         * @brief Returns the number of elements that can be added to the
         * circular buffer before it is full.
         */
        std::size_t get_space_unoccupied() const {
            return this->capacity - this->length;
        }

        /**
         * @brief Returns the number of elements that can be written before
         * written elements wrap back to the beginning of the internal buffer.
         */
        std::size_t get_writes_until_wrap() const {
            return this->capacity - this->write_index;
        }

        /**
         * @brief Returns the number of elements can be read before read
         * elements wrap back to the beginning of the internal buffer.
         */
        std::size_t get_reads_until_wrap() const {
            return this->capacity - this->read_index;
        }

        /**
         * @brief Reads and removes data elements from the circular buffer,
         * assuming that the circular buffer contains enough elements.
         *
         * @pre The circular buffer contains at least @p count data elements.
         * @post The oldest @p count elements are no longer in the circular
         * buffer, but any other elements previously in the circular buffer are
         * still present. The read elements are copied into the @p elements
         * array in order from oldest to newest.
         *
         * @param[out] elements The array into which to copy the read elements.
         * @param count The number of elements to read and remove.
         */
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

        /**
         * @brief Reads and removes data elements from the circular buffer if
         * the circular buffer contains enough elements.
         *
         * This function first checks if the circular buffer contains enough
         * elements to service the read. If it does, then it will read, remove,
         * and copy the elements, just like read_unchecked(), and return true.
         * If not, then it will return false without mutating the circular
         * buffer.
         *
         * @param[out] elements The array into which to copy the read elements.
         * @param count The number of elements to read and remove.
         * @return True if the elements were read, or false if the elements
         * were not read.
         */
        bool read_checked(T* elements, std::size_t count) {
            if (count > this->get_space_occupied()) {
                return false;
            }
            this->read_unchecked(elements, count);
            return true;
        }

        /**
         * @brief Reads and removes up to the requested number of data elements
         * from the circular buffer.
         *
         * This function is similar to read_unchecked(), but if there are fewer
         * than @p count elements in the buffer, it will read all remaining
         * elements in the buffer.
         *
         * @param[out] elements The array into which to copy the read elements.
         * @param count The number of elements to read and remove.
         * @return The number of elements that were read.
         */
        std::size_t read_nonblock(T* elements, std::size_t count) {
            count = std::min(count, this->get_space_occupied());
            this->read_unchecked(elements, count);
            return count;
        }

        /**
         * @brief Adds the provided data elements to the circular buffer,
         * assuming that the circular buffer has space for those elements.
         *
         * @pre The circular buffer has space for at least @p count additional
         * data elements.
         * @post The first @p count elements in the @p elements array are
         * copied into the circular buffer in order from first to last. Any
         * other elements previously in the circular buffer are still present.
         *
         * @param[in] elements The array storing the elements to add.
         * @param count The number of elements to add.
         */
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

        /**
         * @brief Adds the provided data elements to the circular buffer if
         * the circular buffer contains enough elements.
         *
         * This function first checks if the circular buffer contains enough
         * space to service the write. If it does, then it will add (by
         * copying) the elements, just like write_unchecked(), and return true.
         * If not, then it will return false without mutating the circular
         * buffer.
         *
         * @param[in] elements The array storing the elements to add.
         * @param count The number of elements to add.
         * @return True if the elements were added, or false if the elements
         * were not added to the circular buffer.
         */
        bool write_checked(const T* elements, std::size_t count) {
            if (this->get_space_unoccupied() < count) {
                return false;
            }
            this->write_unchecked(elements, count);
            return true;
        }

        /**
         * @brief Adds up to the requested number of data elements to the
         * circular buffer if the circular buffer has space for them.
         *
         * This function is similar to write_unchecked(), but if is only space
         * for fewer than @p count elements in the buffer, it will write
         * elements to the circular buffer until the circular buffer is full.
         *
         * @param[in] elements The array storing the elements to add.
         * @param count The number of elements to add.
         * @return The number of elements that were added.
         */
        std::size_t write_nonblock(const T* elements, std::size_t count) {
            count = std::min(count, this->get_space_unoccupied());
            this->write_unchecked(elements, count);
            return count;
        }

        /**
         * @brief Provides a pointer to space that can be initialized to write
         * elements to the circular buffer without copying them, assuming that
         * the circular buffer has space for those items.
         *
         * The purpose of this function is to provide a zero-copy API to write
         * to the circular buffer. See also finish_write(). This function does
         * not mutate the circular buffer, but the returned pointer is not
         * const.
         *
         * In some cases, one can treat the returned pointer as an array to
         * write multiple elements. This only works if the end of the circular
         * buffer's internal array is not reached; if it is reached, then the
         * written region will be discontiguous. A recommended usage pattern
         * is to fix a batch size in advance, and initialize the circular
         * buffer's capacity to a multiple of the batch size. Then, if all
         * writes are done at the batch size, one need not worry about
         * individual writes being discontiguous. If you do not use this usage
         * pattern, then get_writes_until_wrap() may be useful.
         *
         * @pre The circular buffer is not already full.
         *
         * @return A pointer to the space in the circular buffer's
         * internal memory where the next written element would be stored.
         */
        T* start_write_unchecked() {
            T* buffer = this->data.mapping();
            return &buffer[this->write_index];
        }

        /**
         * @brief Adds elements to the circular buffer in place, without
         * copying them.
         *
         * The recommended usage pattern is to call start_write_unchecked(),
         * initialize the elements to write in place via the returned pointer,
         * and then call this function so that the initialized elements are now
         * considered part of the circular buffer.
         *
         * @pre There is space for at least @p amount elements in the circular
         * buffer.
         * @post The next @p amount elements in the circular buffer's internal
         * memory are added to the circular buffer.
         *
         * @param amount The number of elements to add to the circular buffer.
         */
        void finish_write(std::size_t amount = 1) {
            this->write_index += amount;
            if (this->capacity <= this->write_index) {
                this->write_index -= this->capacity;
            }
            this->length += amount;
        }

        /**
         * @brief Provides a pointer to the oldest element in the circular
         * buffer, so the elements can be read without copying them.
         *
         * The purpose of this function is to provide a zero-copy API to read
         * from the circular buffer without copying them or removing them from
         * the circular buffer. See also finish_write(). This function does
         * not mutate the circular buffer.
         *
         * In some cases, one can treat the returned pointer as an array to
         * read multiple elements. This only works if the end of the circular
         * buffer's internal array is not reached; if it is reached, then the
         * read region will be discontiguous. A recommended usage pattern is to
         * fix a batch size in advance, and initialize the circular buffer's
         * capacity to a multiple of the batch size. Then, if all reads use
         * that same batch size, one need not worry about individual reads
         * being discontiguous. If you do not use this usage pattern, then
         * get_reads_until_wrap() may be useful.
         *
         * @pre The circular buffer is not already full.
         *
         * @return A pointer to the space in the circular buffer's
         * internal memory for the next element to be read.
         */
        const T* start_read_unchecked() const {
            const T* buffer = this->data.mapping();
            return &buffer[this->read_index];
        }

        /**
         * @brief Removes elements from the circular buffer in place, without
         * copying them.
         *
         * The recommended usage pattern is to call start_read_unchecked() to
         * read the desired elements without copying them, and then call this
         * function so that the read elements are no longer considered part of
         * the buffer.
         *
         * @pre There are at least @p amount elements in the circular buffer.
         * @post The @p amount oldest elements in the circular buffer are
         * removed, and any other elements in the circular buffer remain.
         *
         * @param amount The number of elements to remove from the circular
         * buffer.
         */
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
