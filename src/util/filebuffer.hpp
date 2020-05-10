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
        T& start_write() {
            void* rv = this->start_write(sizeof(T));
            return *reinterpret_cast<T*>(rv);
        }

        void* start_write(std::size_t maximum_size) {
            if (maximum_size > this->buffer.size() - this->position) {
                this->flush();
            }
            assert(maximum_size <= this->buffer.size() - this->position);
            return &this->buffer.mapping()[position];
        }

        void finish_write(std::size_t actual_size) {
            this->position += actual_size;
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

    class BufferedFileReader {
    public:
        BufferedFileReader(const char* filename, std::size_t buffer_size = 1 << 18)
            : position(0), buffer(buffer_size, true), eof(buffer_size) {
            this->fd = platform::open_file(filename, nullptr);
        }

        BufferedFileReader(int file_descriptor, std::size_t buffer_size = 1 << 18)
            : fd(file_descriptor), position(0), buffer(buffer_size, true), eof(buffer_size) {
        }

        virtual ~BufferedFileReader() {
            platform::close_file(this->fd);
        }

        template <typename T>
        T* prepare_read() {
            void* rv = this->start_read(sizeof(T));
            return reinterpret_cast<T*>(rv);
        }

        void* start_read(std::size_t maximum_size) {
            if (maximum_size > this->buffer.size() - this->position) {
                this->rebuffer();
            }
            assert(maximum_size > this->buffer.size() - this->position);
            return &this->buffer.mapping()[this->position];
        }

        void commit_read(std::size_t actual_size) {
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
}

#endif
