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

#ifndef MAGE_UTIL_FILEBUFFER_HPP_
#define MAGE_UTIL_FILEBUFFER_HPP_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include "platform/filesystem.hpp"
#include "platform/memory.hpp"

/*
 * This file provides alternatives to the default file stream buffer in
 * libstdc++ that use less CPU time.
 */

namespace mage::util {
    template <bool backwards_readable = false>
    class BufferedFileWriter {
    public:
        BufferedFileWriter(const char* filename, std::size_t buffer_size = 1 << 18)
            : position(0), buffer(buffer_size, true) {
            this->fd = platform::create_file(filename, 0);
        }

        BufferedFileWriter(int file_descriptor, std::size_t buffer_size = 1 << 18)
            : fd(file_descriptor), position(0), buffer(buffer_size, true) {
        }

        virtual ~BufferedFileWriter() {
            this->flush();
            platform::close_file(this->fd);
        }

        template <typename T>
        T& write(std::size_t size = sizeof(T)) {
            void* rv = this->start_write(size);
            this->finish_write(size);
            return *reinterpret_cast<T*>(rv);
        }

        template <typename T>
        T& start_write(std::size_t maximum_size = sizeof(T)) {
            void* rv = this->start_write(maximum_size);
            return *reinterpret_cast<T*>(rv);
        }

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

        void finish_write(std::size_t actual_size) {
            this->position += actual_size;
            if constexpr(backwards_readable) {
                this->buffer.mapping()[this->position] = static_cast<std::uint8_t>(actual_size);
                this->position++;
            }
        }

        void flush() {
            platform::write_to_file(this->fd, this->buffer.mapping(), this->position);
            this->position = 0;
        }

    protected:
        int fd;

    private:
        std::size_t position;
        platform::MappedFile<std::uint8_t> buffer;
    };

    template <bool backwards_readable>
    class BufferedFileReader {
    public:
        BufferedFileReader(const char* filename, std::size_t buffer_size = 1 << 18)
            : position(buffer_size), buffer(buffer_size, true), eof(buffer_size) {
            this->fd = platform::open_file(filename, nullptr);
        }

        BufferedFileReader(int file_descriptor, std::size_t buffer_size = 1 << 18)
            : fd(file_descriptor), position(buffer_size), buffer(buffer_size, true), eof(buffer_size) {
        }

        virtual ~BufferedFileReader() {
            platform::close_file(this->fd);
        }

        template <typename T>
        T& start_read(std::size_t maximum_size = sizeof(T)) {
            void* rv = this->start_read(maximum_size);
            return *reinterpret_cast<T*>(rv);
        }

        void* start_read(std::size_t maximum_size) {
            if constexpr(backwards_readable) {
                maximum_size += 1;
            }
            if (maximum_size > this->buffer.size() - this->position) {
                this->rebuffer();
            }
            assert(maximum_size <= this->buffer.size() - this->position);
            return &this->buffer.mapping()[this->position];
        }

        void finish_read(std::size_t actual_size) {
            if constexpr(backwards_readable) {
                actual_size += 1;
            }
            this->position += actual_size;
            if (this->position == this->buffer.size()) {
                this->rebuffer();
            } else if (this->position > this->eof) {
                std::abort();
            }
        }

        bool at_eof() const {
            return this->position == this->eof;
        }

        void rebuffer() {
            std::uint8_t* mapping = this->buffer.mapping();
            std::size_t size = this->buffer.size();
            std::size_t leftover = size - this->position;
            std::copy(&mapping[this->position], &mapping[size], mapping);
            this->eof = leftover + platform::read_from_file(this->fd, &mapping[leftover], this->position);
            this->position = 0;
        }

    protected:
        int fd;

    private:
        std::size_t position;
        std::size_t eof;
        platform::MappedFile<std::uint8_t> buffer;
    };

    template <bool backwards_readable>
    class BufferedReverseFileReader {
        static_assert(backwards_readable);
    public:
        BufferedReverseFileReader(const char* filename, std::size_t buffer_size = 1 << 18)
            : position(0), buffer(buffer_size, true) {
            this->fd = platform::open_file(filename, &this->length_left);
        }

        BufferedReverseFileReader(int file_descriptor, std::size_t buffer_size = 1 << 18)
            : fd(file_descriptor), position(0), buffer(buffer_size, true) {
        }

        virtual ~BufferedReverseFileReader() {
            platform::close_file(this->fd);
        }

        template <typename T>
        T& read(std::size_t& size) {
            void* rv = this->read(size);
            return *reinterpret_cast<T*>(rv);
        }

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

        void rebuffer() {
            std::uint8_t* mapping = this->buffer.mapping();
            std::size_t size = this->buffer.size();
            std::uint64_t to_read = size - this->position;
            to_read = std::min(to_read, this->length_left);
            std::copy_backward(mapping, &mapping[this->position], &mapping[to_read + this->position]);

            this->position += to_read;
            this->length_left -= to_read;
            platform::seek_file(this->fd, this->length_left);
            platform::read_from_file(this->fd, mapping, to_read);
        }

    protected:
        int fd;
        std::uint64_t length_left;

    private:
        std::size_t position;
        platform::MappedFile<std::uint8_t> buffer;
    };
}

#endif
