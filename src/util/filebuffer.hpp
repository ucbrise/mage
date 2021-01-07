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
 * @file util/filebuffer.hpp
 * @brief Efficient in-memory buffering for file descriptors.
 */

#ifndef MAGE_UTIL_FILEBUFFER_HPP_
#define MAGE_UTIL_FILEBUFFER_HPP_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <utility>
#include "platform/filesystem.hpp"
#include "platform/memory.hpp"
#include "util/stats.hpp"

/*
 * This file provides alternatives to the default file stream buffer in
 * libstdc++ that use less CPU time.
 */

namespace mage::util {
    /**
     *  @brief Wrapper for a file descriptor, providing in-memory buffering and
     * an API that allows zero-copy writes.
     *
     * Initially, MAGE's planning used libstdc++ (std::ifstream and
     * std::ofstream) to handle file I/O. It became clear when profiling the
     * code, however, that libstdc++'s file I/O utilities consumed a
     * significant amount of CPU time. That prompted the switch to these
     * more efficient utilities.
     *
     * @tparam backwards_readable If false, the stream is treated as a sequence
     * of bytes; it is understood as a sequence of items, where each write
     * corresponds to one item, but the boundaries between items are not
     * recorded in the underlying file descriptor. If true, markers are
     * written to the underlying file descriptor so that one can identify the
     * items and efficiently iterate over them in reverse order.
     */
    template <bool backwards_readable = false>
    class BufferedFileWriter {
    public:
        /**
         * @brief Creates a BufferedFileWriter without a valid underlying file
         * descriptor.
         *
         * The created BufferedFileWriter should not be used until a file
         * descriptor is provided to it by calling set_file_descriptor().
         *
         * @param buffer_size Size of the in-memory buffer.
         */
        BufferedFileWriter(std::size_t buffer_size = 1 << 18)
            : fd(-1), owns_fd(false), use_stats(false), position(0), buffer(buffer_size, true) {
        }

        /**
         * @brief Creates the file with the specified name and creates a
         * BufferedFileWriter that writes to it and has ownership of the
         * associated file descriptor.
         *
         * The created BufferedFileWriter can be used to write to the file
         * immediately.
         *
         * @param buffer_size Size of the in-memory buffer.
         */
        BufferedFileWriter(const char* filename, std::size_t buffer_size = 1 << 18)
            : owns_fd(true), use_stats(false), position(0), buffer(buffer_size, true) {
            this->fd = platform::create_file(filename, 0);
        }

        /**
         * @brief Creates a BufferedFileWriter that writes to the specified
         * file descriptor.
         *
         * The created BufferedFileWriter can be used to write to the file
         * descriptor immediately.
         *
         * @param file_descriptor The file descriptor to use.
         * @param owns_fd If true, this BufferedFileWriter "owns" the provided
         * file descriptor, meaning that it is responsible for closing it.
         * @param buffer_size Size of the in-memory buffer.
         */
        BufferedFileWriter(int file_descriptor, bool owns_fd, std::size_t buffer_size = 1 << 18)
            : BufferedFileWriter(buffer_size) {
            this->set_file_descriptor(file_descriptor, owns_fd);
        }

        /**
         * @brief Move constructor for BufferedFileWriter.
         */
        BufferedFileWriter(BufferedFileWriter<backwards_readable>&& other)
            : fd(other.fd), owns_fd(other.owns_fd), use_stats(other.use_stats),
            position(other.position), buffer(std::move(other.buffer)) {
            other.fd = -1;
            other.owns_fd = false;
            other.use_stats = false;
            other.position = 0;
        }

        /**
         * @brief Sets the underlying file descriptor.
         *
         * @param file_descriptor The new file descriptor to use.
         * @param owns_fd If true, this BufferedFileWriter "owns" the provided
         * file descriptor, meaning that it is responsible for closing it.
         */
        void set_file_descriptor(int file_descriptor, bool owns_fd) {
            this->fd = file_descriptor;
            this->owns_fd = owns_fd;
        }

        /**
         * @brief Returns the underlying file descriptor, gives up ownership of
         * it, and stops using it.
         *
         * After this function is called, this BufferedFileWriter should not be
         * used until a new file descriptor is provided, by Calling
         * set_file_descriptor().
         */
        int relinquish_file_descriptor() {
            if (this->fd == -1) {
                return -1;
            }
            this->flush();
            int old_fd = this->fd;
            this->fd = -1;
            this->owns_fd = false;
            return old_fd;
        }

        /**
         * @brief Flushes any in-memory data to the underlying file descriptor
         * and closes the underlying file descriptor, if this
         * BufferedFileWriter has ownership of it.
         */
        virtual ~BufferedFileWriter() {
            if (this->fd != -1) {
                if (this->position != 0) {
                    this->flush();
                }
                if (this->owns_fd) {
                    platform::close_file(this->fd);
                }
            }
        }

        /**
         * @brief Enables collection of statistics for rebuffer times.
         *
         * @param label Label to use for statistics collection.
         */
        void enable_stats(const std::string& label) {
            this->use_stats = true;
            this->stats.set_label(label);
        }

        /**
         * @brief Returns statistics collector for this BufferedFileWriter.
         */
        util::StreamStats& get_stats() {
            return this->stats;
        }

        /**
         * @brief Provides a reference to a spot in this BufferedFileWriter's
         * internal buffer where the next item can be initialized, and advances
         * the stream to the next item.
         *
         * Calling this function may invalidate any prior references or
         * pointers obtained by calling write() or start_write().
         *
         * @tparam T Type of the item; the returned reference is of type T&.
         * @param size The size of the item.
         * @return Reference to the item to initialize.
         */
        template <typename T>
        T& write(std::size_t size = sizeof(T)) {
            void* rv = this->start_write(size);
            this->finish_write(size);
            return *reinterpret_cast<T*>(rv);
        }

        /**
         * @brief Provides a reference to a spot in this BufferedFileWriter's
         * internal buffer where the next item can be initialized, without
         * advancing the stream's position.
         *
         * Calling this function may invalidate any prior references or
         * pointers obtained by calling write() or start_write().
         *
         * @tparam T Type of the item; the returned reference is of type T&.
         * @param size The size of the item.
         * @return Reference to the item to initialize.
         */
        template <typename T>
        T& start_write(std::size_t maximum_size = sizeof(T)) {
            void* rv = this->start_write(maximum_size);
            return *reinterpret_cast<T*>(rv);
        }

        /**
         * @brief Provides a pointer to a spot in this BufferedFileWriter's
         * internal buffer where the next item can be initialized, without
         * advancing the stream's position.
         *
         * Calling this function may invalidate any prior references or
         * pointers obtained by calling write() or start_write().
         *
         * @param size The size of the item.
         * @return Pointer to the item to initialize.
         */
        void* start_write(std::size_t maximum_size) {
            if constexpr(backwards_readable) {
                maximum_size += 1;
            }
            if (maximum_size > this->buffer.size() - this->position) {
                this->flush();
            }
            assert(maximum_size <= this->buffer.size() - this->position);
            return &this->buffer.mapping()[this->position];
        }

        /**
         * @brief Advances the stream by the specified number of bytes.
         *
         * The recommended usage is to first call start_write() with the
         * maximum size of of the next item to obtain a reference/pointer to
         * the next item. One can then initialize the item direcly in this
         * BufferedFileReader's internal buffer, and in the process, determine
         * its actual size. Then, one can call finish_write() with the actual
         * size of the item to advance the stream past this item.
         *
         * @param actual_size Number of bytes by which to advance the stream.
         */
        void finish_write(std::size_t actual_size) {
            this->position += actual_size;
            if constexpr(backwards_readable) {
                this->buffer.mapping()[this->position] = static_cast<std::uint8_t>(actual_size);
                this->position++;
            }
        }

        /* After calling this, the buffer provided on write will be aligned. */

        /**
         * @brief Empties the in-memory buffer, writing any pending data to the
         * underlying file descriptor.
         *
         * The write() and start_write() functions will call flush() as needed
         * when the in-memory buffer fills. In cases when data must be written
         * to the underlying file descriptor automatically (e.g., for a
         * network protocol involving multiple round trips), the user of this
         * class should call flush().
         *
         * Another case where it may be useful to call this function manually
         * is for alignment; the start of the internal buffer is guaranteed to
         * be page-aligned. Thus, if the next sequence of items is known to
         * require alignment to use efficiently, and are aligned correctly
         * relative to each other, it could be desirable to call this function
         * once, to align the sequence of items to a page boundary, before
         * initializing them.
         *
         * Calling this function resets this BufferedFileWriter's position in
         * its internal buffer, invalidating all pointers and references
         * returned write() and start_write(). This function writes data to the
         * underlying file descriptor, which can be expensive if done
         * frequently. Thus, one should be careful when calling this function
         * externally.
         */
        void flush() {
            if (this->use_stats) {
                auto start = std::chrono::steady_clock::now();

                this->_flush();

                auto end = std::chrono::steady_clock::now();
                this->stats.event(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
            } else {
                this->_flush();
            }
        }

    private:
        void _flush() {
            platform::write_to_file(this->fd, this->buffer.mapping(), this->position);
            this->position = 0;
        }

    protected:
        int fd;
        bool owns_fd;
        bool use_stats;
        util::StreamStats stats;

    private:
        std::size_t position;
        platform::MappedFile<std::uint8_t> buffer;
    };

    /**
     *  @brief Wrapper for a file descriptor, providing in-memory buffering and
     * an API that allows zero-copy reads in forward order.
     *
     * Initially, MAGE's planning used libstdc++ (std::ifstream and
     * std::ofstream) to handle file I/O. It became clear when profiling the
     * code, however, that libstdc++'s file I/O utilities consumed a
     * significant amount of CPU time. That prompted the switch to these
     * more efficient utilities.
     *
     * @tparam backwards_readable If false, the stream is assumed to be a raw
     * sequence of bytes. If true, the stream is treated as a sequence of
     * items with interspersed size markers, in the same format as would be
     * written by BufferedFileWriter\<true\>.
     */
    template <bool backwards_readable>
    class BufferedFileReader {
    public:
        /**
         * @brief Creates a BufferedFileReader without a valid underlying file
         * descriptor.
         *
         * The created BufferedFileReader should not be used until a file
         * descriptor is provided to it by calling set_file_descriptor().
         *
         * @param buffer_size Size of the in-memory buffer.
         */
        BufferedFileReader(std::size_t buffer_size = 1 << 18)
            : fd(-1), owns_fd(false), use_stats(false), position(0), buffer(buffer_size, true), active_size(0) {
        }

        /**
         * @brief Opens the file with the specified name and creates a
         * BufferedFileReader that reads from it and has ownership of the
         * associated file descriptor.
         *
         * The created BufferedFileReader can be used to read from the file
         * immediately.
         *
         * @param buffer_size Size of the in-memory buffer.
         */
        BufferedFileReader(const char* filename, std::size_t buffer_size = 1 << 18)
            : owns_fd(true), use_stats(false), position(0), buffer(buffer_size, true), active_size(0) {
            this->fd = platform::open_file(filename, nullptr);
        }

        /**
         * @brief Creates a BufferedFileReader that reads from the specified
         * file descriptor.
         *
         * The created BufferedFileReader can be used to read from the file
         * descriptor immediately.
         *
         * @param file_descriptor The file descriptor to use.
         * @param owns_fd If true, this BufferedFileReader "owns" the provided
         * file descriptor, meaning that it is responsible for closing it.
         * @param buffer_size Size of the in-memory buffer.
         */
        BufferedFileReader(int file_descriptor, bool owns_fd, std::size_t buffer_size = 1 << 18)
            : BufferedFileReader(buffer_size) {
            this->set_file_descriptor(file_descriptor, owns_fd);
        }

        /**
         * @brief Move constructor for BufferedFileReader.
         */
        BufferedFileReader(BufferedFileReader<backwards_readable>&& other)
            : fd(other.fd), owns_fd(other.owns_fd), use_stats(other.use_stats), position(other.position),
            buffer(std::move(other.buffer)), active_size(other.active_size) {
            other.fd = -1;
            other.owns_fd = false;
            other.use_stats = false;
            other.position = 0;
            other.active_size = 0;
        }

        /**
         * @brief Sets the underlying file descriptor.
         *
         * @param file_descriptor The new file descriptor to use.
         * @param owns_fd If true, this BufferedFileReader "owns" the provided
         * file descriptor, meaning that it is responsible for closing it.
         */
        void set_file_descriptor(int file_descriptor, bool owns_fd) {
            this->fd = file_descriptor;
            this->owns_fd = owns_fd;
        }

        /**
         * @brief Returns the underlying file descriptor, gives up ownership of
         * it, and stops using it.
         *
         * After this function is called, this BufferedFileReader should not be
         * used until a new file descriptor is provided, by Calling
         * set_file_descriptor().
         */
        int relinquish_file_descriptor() {
            int old_fd = this->fd;
            this->fd = -1;
            this->owns_fd = false;
            return old_fd;
        }

        /**
         * @brief Closes the underlying file descriptor, if this
         * BufferedFileReader has ownership of it.
         */
        virtual ~BufferedFileReader() {
            if (this->owns_fd) {
                platform::close_file(this->fd);
            }
        }

        /**
         * @brief Enables collection of statistics for rebuffer times.
         *
         * @param label Label to use for statistics collection.
         */
        void enable_stats(const std::string& label) {
            this->use_stats = true;
            this->stats.set_label(label);
        }

        /**
         * @brief Returns the statistics collector for this BufferedFileReader.
         */
        util::StreamStats& get_stats() {
            return this->stats;
        }

        /**
         * @brief Provides a reference to the next item in the stream and
         * advances the stream past that item.
         *
         * Calling this function may invalidate any prior references or
         * pointers obtained by calling read() or start_read().
         *
         * @tparam T Type of the item; the returned reference is of type T&.
         * @param size The size of the item.
         * @return Reference to the next item.
         */
        template <typename T>
        T& read(std::size_t size = sizeof(T)) {
            T& rv = this->start_read<T>(size);
            this->finish_read(size);
            return rv;
        }

        /**
         * @brief Provides a reference to the next item in the stream without
         * advancing the stream's position.
         *
         * Calling this function may invalidate any prior references or
         * pointers obtained by calling read() or start_read().
         *
         * @tparam T Type of the item; the returned reference is of type T&.
         * @param size An upper bound on the size of the item; data is read
         * from the underlying file descriptor, as necessary, to ensure that
         * at least this much continuous data is available.
         * @return Reference to the next item.
         */
        template <typename T>
        T& start_read(std::size_t maximum_size = sizeof(T)) {
            void* rv = this->start_read(maximum_size);
            return *reinterpret_cast<T*>(rv);
        }

        /**
         * @brief Provides a pointer to the next item in the stream without
         * advancing the stream's position.
         *
         * Calling this function may invalidate any prior references or
         * pointers obtained by calling read() or start_read().
         *
         * @param size An upper bound on the size of the item; data is read
         * from the underlying file descriptor, as necessary, to ensure that
         * at least this much continuous data is available.
         * @return Pointer to the next item.
         */
        void* start_read(std::size_t maximum_size) {
            if constexpr(backwards_readable) {
                maximum_size += 1;
            }
            while (maximum_size > this->active_size - this->position && this->rebuffer()) {
            }
            return &this->buffer.mapping()[this->position];
        }

        /**
         * @brief Advances the stream by the specified number of bytes.
         *
         * The recommended usage is to first call start_read() with the maximum
         * size of of the next item to obtain a reference/pointer to the next
         * item. One can then read that item without copying it from the
         * BufferedFileReader's internal buffer, and in the process, determine
         * its actual size. Then, one can call finish_read() with the actual
         * size of the item to advance the stream to the start of the next
         * item.
         *
         * @param actual_size Number of bytes by which to advance the stream.
         */
        void finish_read(std::size_t actual_size) {
            if constexpr(backwards_readable) {
                actual_size += 1;
            }
            this->position += actual_size;
        }

        /**
         * @brief Refreshes the in-memory buffer by reading from the underlying
         * file descriptor.
         *
         * Typically, the read() and start_read() functions will call
         * rebuffer() as appropriate, and the user of this class will not have
         * to call this function manually. One case where it may be useful to
         * call this function manually is for alignment; the start of the
         * internal buffer is guaranteed to be page-aligned. Thus, if the next
         * sequence of items is known to require alignment to use efficiently,
         * and are aligned correctly relative to each other, it could be
         * desirable to call this function once, to align the sequence of items
         * to a page boundary, before reading the items.
         *
         * This moves data within the internal buffer, invalidating all
         * pointers and references returned by previous calls to read(). It is
         * quite cheap when the buffer is nearly empty (which is usually the
         * case when it is called internally), but can be relatively expensive
         * when the buffer is not nearly empty. Additionally, it reads data
         * from the underlying file descriptor, which can be expensive when
         * done frequently. Thus, one should be careful when calling this
         * function externally.
         */
        bool rebuffer() {
            if (this->use_stats) {
                auto start = std::chrono::steady_clock::now();

                bool rv = this->_rebuffer();
//
                auto end = std::chrono::steady_clock::now();
                this->stats.event(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

                return rv;
            } else {
                return this->_rebuffer();
            }
        }

    private:
        bool _rebuffer() {
            std::uint8_t* mapping = this->buffer.mapping();
            std::size_t leftover = this->active_size - this->position;
            std::copy(&mapping[this->position], &mapping[this->active_size], mapping);
            std::size_t rv = platform::read_available_from_file(this->fd, &mapping[leftover], this->buffer.size() - leftover);
            this->active_size = leftover + rv;
            this->position = 0;
            return rv != 0;
        }

    protected:
        int fd;
        bool owns_fd;
        bool use_stats;
        util::StreamStats stats;

    private:
        std::size_t active_size;
        std::size_t position;
        platform::MappedFile<std::uint8_t> buffer;
    };

    /**
     *  @brief Wrapper for a file descriptor, providing in-memory buffering and
     * an API that allows zero-copy reads in reverse order.
     *
     * The stream is interpreted as a sequence of items with interspersed size
     * markers, as would be written by BufferedFileWriter\<true\>.
     *
     * @tparam backwards_readable Must be true.
     */
    template <bool backwards_readable>
    class BufferedReverseFileReader {
        static_assert(backwards_readable);
    public:
        /**
         * @brief Opens the file with the specified name and creates a
         * BufferedReverseFileReader that reads from it and has ownership of
         * the associated file descriptor.
         *
         * The created BufferedFileReader can be used to read from the file
         * immediately.
         *
         * @param buffer_size Size of the in-memory buffer.
         */
        BufferedReverseFileReader(const char* filename, std::size_t buffer_size = 1 << 18)
            : owns_fd(true), position(0), buffer(buffer_size, true) {
            this->fd = platform::open_file(filename, &this->length_left);
        }

        /**
         * @brief Creates a BufferedFileReader that reads from the specified
         * file descriptor but does not have ownership of the file descriptor.
         *
         * The created BufferedFileReader can be used to read from the file
         * descriptor immediately.
         *
         * @param buffer_size Size of the in-memory buffer.
         */
        BufferedReverseFileReader(int file_descriptor, std::size_t buffer_size = 1 << 18)
            : fd(file_descriptor), owns_fd(false), length_left(UINT64_MAX), position(0), buffer(buffer_size, true) {
        }

        /**
         * @brief Closes the underlying file descriptor, if this
         * BufferedReverseFileReader has ownership of it.
         */
        virtual ~BufferedReverseFileReader() {
            if (this->owns_fd) {
                platform::close_file(this->fd);
            }
        }

        /**
         * @brief Provides a reference to the next item in the stream (where
         * "next" means "next in reverse order") and advances the stream's
         * position.
         *
         * Calling this function may invalidate any prior references or
         * pointers obtained by calling read().
         *
         * @tparam T Type of the item; the returned reference is of type T&.
         * @param[out] size Initialized to the size of the item.
         * @return Reference to the next item.
         */
        template <typename T>
        T& read(std::size_t& size) {
            void* rv = this->read(size);
            return *reinterpret_cast<T*>(rv);
        }

        /**
         * @brief Provides a pointer to the next item in the stream (where
         * "next" means "next in reverse order") and advances the stream's
         * position.
         *
         * Calling this function may invalidate any prior references or
         * pointers obtained by calling read().
         *
         * @param[out] size Initialized to the size of the item.
         * @return Pointer to the next item.
         */
        void* read(std::size_t& size) {
            std::uint8_t* mapping = this->buffer.mapping();
            if (this->position == 0) {
                this->rebuffer();
            }
            assert(this->position != 0);
            this->position--;
            size = mapping[this->position];
            if (size > this->position) {
                this->rebuffer();
            }
            assert(size <= this->position);
            this->position -= size;
            return &mapping[this->position];
        }

    private:
        /**
         * @brief Refreshes the in-memory buffer by reading from the underlying
         * file descriptor.
         *
         * This may move data within the internal buffer, invalidating all
         * pointers and references returned by previous calls to read().
         */
        void rebuffer() {
            std::uint8_t* mapping = this->buffer.mapping();
            std::size_t size = this->buffer.size() - slack;
            std::uint64_t to_read = size - this->position;
            to_read = std::min(to_read, this->length_left);
            std::copy_backward(mapping, &mapping[this->position], &mapping[to_read + this->position]);

            this->position += to_read;
            this->length_left -= to_read;
            platform::read_from_file_at(this->fd, mapping, to_read, this->length_left);
        }

    protected:
        int fd;
        bool owns_fd;
        std::uint64_t length_left;

    private:
        std::size_t position;
        platform::MappedFile<std::uint8_t> buffer;

        /*
         * We need to rebuffer a few bytes early because at some optimization
         * levels, the code for reading a bitfield accesses a few bytes past
         * the struct.
         */
        static constexpr const std::size_t slack = 7;
    };
}

#endif
