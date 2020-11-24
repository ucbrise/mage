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

#ifndef MAGE_UTIL_BINARYFILE_HPP_
#define MAGE_UTIL_BINARYFILE_HPP_

#include <cstdint>
#include <algorithm>
#include "platform/filesystem.hpp"
#include "util/filebuffer.hpp"

namespace mage::util {
    class BinaryFileWriter : private BufferedFileWriter<false> {
    public:
        BinaryFileWriter(const char* output_file) : BufferedFileWriter<false>(output_file), total_num_bits(0), current_byte(0) {
        }

        ~BinaryFileWriter() {
            if ((static_cast<std::uint8_t>(this->total_num_bits) & 0x7) != 0) {
                this->write<std::uint8_t>() = this->current_byte;
            }
        }

        void write64(std::uint64_t value) {
            for (int i = 0; i != 8; i++) {
                this->write8(static_cast<std::uint8_t>(value));
                value >>= 8;
            }
        }

        void write_double(double value) {
            std::uint64_t* ptr = reinterpret_cast<std::uint64_t*>(&value);
            this->write64(*ptr);
        }

        void write32(std::uint32_t value) {
            for (int i = 0; i != 4; i++) {
                this->write8(static_cast<std::uint8_t>(value));
                value >>= 8;
            }
        }

        void write_float(float value) {
            std::uint32_t* ptr = reinterpret_cast<std::uint32_t*>(&value);
            this->write32(*ptr);
        }

        void write16(std::uint16_t value) {
            for (int i = 0; i != 2; i++) {
                this->write8(static_cast<std::uint8_t>(value));
                value >>= 8;
            }
        }

        void write8(std::uint8_t byte) {
            std::uint8_t current_bit = static_cast<std::uint8_t>(this->total_num_bits) & 0x7;
            this->current_byte |= (byte << current_bit);
            this->write<std::uint8_t>() = this->current_byte;
            this->current_byte = (byte >> (8 - current_bit));
            this->total_num_bits += 8;
        }

        void write1(std::uint8_t bit) {
            std::uint8_t current_bit = static_cast<std::uint8_t>(this->total_num_bits) & 0x7;
            this->current_byte |= (bit << current_bit);
            this->total_num_bits++;
            if (current_bit == 7) {
                this->write<std::uint8_t>() = this->current_byte;
                this->current_byte = 0;
            }
        }

    private:
        std::uint64_t total_num_bits;
        std::uint8_t current_byte;
    };

    class BinaryFileReader : private BufferedFileReader<false> {
    public:
        BinaryFileReader(const char* input_file, std::size_t buffer_size = 1 << 18) : BufferedFileReader<false>(input_file, buffer_size), current_bit(0), current_byte(0) {
        }

        std::uint64_t get_file_length() const {
            return platform::length_file(this->fd);
        }

        std::uint8_t read1() {
            if (this->current_bit == 0) {
                this->current_byte = this->read<std::uint8_t>();
            }
            std::uint8_t bit = (this->current_byte >> this->current_bit) & 0x1;
            this->current_bit = (this->current_bit + 1) & 0x7;
            return bit;
        }

        template <typename T>
        T read() {
            T rv;
            std::uint8_t* ptr = reinterpret_cast<std::uint8_t*>(&rv);
            this->read_bytes(ptr, sizeof(T));
            return rv;
        }

        void read_bytes(std::uint8_t* bytes, std::size_t num_bytes) {
            if (this->current_bit == 0) {
                void* from = this->start_read(num_bytes);
                std::uint8_t* from_bytes = static_cast<std::uint8_t*>(from);
                std::copy(from_bytes, from_bytes + num_bytes, bytes);
                this->finish_read(num_bytes);
            } else {
                for (std::uint32_t i = 0; i != num_bytes; i++) {
                    bytes[i] = (this->current_byte >> this->current_bit);
                    this->current_byte = this->read<std::uint8_t>();
                    bytes[i] |= (this->current_byte << (8 - this->current_bit));
                }
            }
        }

        void read_bits(std::uint8_t* bytes, std::size_t num_bits) {
            std::size_t num_bytes = num_bits >> 3;
            this->read_bytes(bytes, num_bytes);
            std::uint8_t leftover_bits = (num_bits & 0x7);
            if (leftover_bits != 0) {
                /*
                 * In many cases, this logic copies more bits than necessary,
                 * but that's OK and cheaper to do.
                 */
                if (this->current_bit == 0) {
                    this->current_byte = this->read<std::uint8_t>();
                }
                bytes[num_bytes] = (this->current_byte >> this->current_bit);
                if (leftover_bits > (8 - this->current_bit)) {
                    this->current_byte = this->read<std::uint8_t>();
                    bytes[num_bytes] |= (this->current_byte << (8 - this->current_bit));
                }
                this->current_bit = (this->current_bit + leftover_bits) & 0x7;
            }
        }

    private:
        std::uint8_t current_bit;
        std::uint8_t current_byte;
    };
}

#endif
