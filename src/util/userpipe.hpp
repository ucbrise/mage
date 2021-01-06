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
    /**
     * @brief A synchronized bounded buffer container.
     *
     * A UserPipe provides a bounded buffer whose capacity is fixed upon
     * initialization. Writing data to the buffer blocks until space is
     * available for the data. Reading data from the buffer blocks until the
     * buffer has the requested amount of data. A UserPipe can also be closed,
     * preventing more data from being written to the buffer and forcing any
     * outstanding reads to stop blocking and return short.
     *
     * Semantically, a UserPipe is a userspace abstraction similar to UNIX
     * pipes (hence the name UserPipe). It is also similar to channels in the
     * Go programming language.
     *
     * UserPipe is based on the CircularBuffer abstraction. Thus, the
     * type @p T of elements in the UserPipe are subject to the same
     * constraints as the CircularBuffer type.
     *
     * @tparam T The type of each element (datum) in the buffer.
     */
    template <typename T>
    class UserPipe : private CircularBuffer<T> {
    public:
        /**
         * @brief Creates a user pipe.
         *
         * @param capacity Number of data elements that can be held.
         */
        UserPipe(std::size_t capacity) : CircularBuffer<T>(capacity), closed(false) {
        }

        /**
         * @brief Disallows further writes to the pipe.
         *
         * Reads can still get what was previously written to the pipe, but
         * they will no longer block waiting for more data.
         */
        void close() {
            std::lock_guard<std::mutex> lock(this->mutex);
            this->closed = true;
            this->added.notify_all();
            this->removed.notify_all();
        }

        /**
         * @brief Reads and removes data elements from the circular buffer,
         * first waiting until the pipe contains the required number of data
         * elements.
         *
         * The read data elements are copied into the @p elements array. If the
         * pipe is closed before the required number of data elements are
         * present, then all elements remaining in the pipe are read.
         *
         * @param[out] elements The array into which to copy the read elements.
         * @param count The number of elements to read and remove.
         * @return The number of elements actually read (could be less than
         * @p count if the pipe is closed concurrently with the read).
         */
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

        /**
         * @brief Adds the provided data elements to the circular buffer,
         * first waiting until the pipe has enough free space for the added
         * data elements.
         *
         * The data elements added by being copied from the @p elements array.
         * If the pipe is closed before the required free space is present,
         * then no data elements are added.
         *
         * @param[in] elements The array storing the elements to add.
         * @param count The number of elements to add.
         * @return True if the elements were added, or false if they were not.
         */
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

        /**
         * @brief Waits for @p amount elements to become available in the pipe,
         * and then provides a pointer to the oldest element in the pipe, so
         * that elements can be read without copying them.
         *
         * The purpose of this function is to provide a zero-copy API to read
         * from the pipe. See also finish_read_in_place().
         *
         * In some cases, one can treat the returned pointer as an array to
         * write multiple elements. The same caveats apply here as in
         * CircularBuffer::start_read_unchecked.
         *
         * The pipe's lock is not held once start_read_in_place returns, and
         * this function does not advance the pipe's read position. Thus, if
         * one thread calls this function, and then another thread calls this
         * function immediately afterwards, then both may receive the same
         * pointer. If that is not the desired behavior, the user should
         * use external synchronization as appropriate.
         *
         * @return A pointer to the space in the pipe's internal memory for the
         * next element to be read, or a null pointer if the pipe is closed
         * before @p amount elements are available in the pipe.
         */
        const T* start_read_in_place(std::size_t amount) {
            std::unique_lock<std::mutex> lock(this->mutex);
            while (this->get_space_occupied() < amount && !this->closed) {
                this->added.wait(lock);
            }
            if (this->get_space_occupied() < amount) {
                return nullptr;
            }
            return this->start_read_unchecked();
        }

        /**
         * @brief Removes elements from the pipe in place, without copying
         * them.
         *
         * The recommended usage pattern is to call start_read_in_place() to
         * read the desired elements without copying them, and then call this
         * function to remove those elements from the pipe.
         *
         * @pre There are at least @p amount elements in the pipe.
         * @post The @p amount oldest elements in the pipe are removed, and any
         * other elements in the pipe remain.
         *
         * @param amount The number of elements to remove from the pipe.
         */
        void finish_read_in_place(std::size_t amount) {
            std::lock_guard<std::mutex> lock(this->mutex);
            assert(this->get_space_occupied() >= amount);
            this->finish_read(amount);
            this->removed.notify_all();
        }

        /**
         * @brief Waits for @p amount elements of free space to become
         * available, and then provides a pointer to space in the pipe's
         * internal buffer for writing @p elements.
         *
         * The purpose of this function is to provide a zero-copy API to write
         * to the pipe. See also finish_write_in_place().
         *
         * In some cases, one can treat the returned pointer as an array to
         * write multiple elements. The same caveats apply here as in
         * CircularBuffer::start_write_unchecked.
         *
         * The pipe's lock is not held once start_write_in_place returns, and
         * this function does not advance the write position of the pipe. Thus,
         * the user must ensure that two different threads do not concurrently
         * write to the same memory in the pipe.
         *
         * @return A pointer to the space in the pipe's internal memory where
         * the next written element would be stored, or a null pointer if
         * the pipe is closed before free space for @p elements is available.
         */
        T* start_write_in_place(std::size_t amount) {
            std::unique_lock<std::mutex> lock(this->mutex);
            while (this->get_space_unoccupied() < amount && !this->closed) {
                this->removed.wait(lock);
            }
            if (this->closed) {
                return nullptr;
            }
            return this->start_write_unchecked();
        }

        /**
         * @brief Adds elements to the circular buffer in place, without
         * copying them.
         *
         * The recommended usage pattern is to call start_write_in_place(),
         * initialize the elements to write in place via the returned pointer,
         * and then call this function so that the initialized elements are now
         * considered part of the pipe.
         *
         * @pre There is space for at least @p amount elements in the pipe.
         * @post The next @p amount elements in the pipe's internal memory are
         * added to the pipe.
         *
         * @param amount The number of elements to add to the pipe.
         */
        void finish_write_in_place(std::size_t amount = 1) {
            std::lock_guard<std::mutex> lock(this->mutex);
            assert(this->get_space_unoccupied() != 0);
            this->finish_write(amount);
            this->added.notify_all();
        }

        /* Interface for synchronization based on internal events. */

        /**
         * @brief Acquires the pipe's internal lock, causing all operations on
         * the pipe to block until the lock is released.
         */
        std::unique_lock<std::mutex> lock() {
            return std::unique_lock<std::mutex>(&mutex);
        }

        /**
         * @brief Waits for an element to be added to the pipe (or for the pipe
         * to be closed), releasing the pipe's internal lock. Should be called
         * in a while loop, like a condition variable.
         *
         * @pre The pipe's internal lock is held by the calling thread.
         */
        void wait_for_addition(std::unique_lock<std::mutex>& lock) {
            this->added.wait(lock);
        }

        /**
         * @brief Waits for an element to be removed from the pipe (or for the
         * pipe to be closed), releasing the pipe's internal lock. Should be
         * called in a while loop, like a condition variable.
         *
         * @pre The pipe's internal lock is held by the calling thread.
         */
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
