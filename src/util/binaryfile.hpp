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

#include <cstdint>
#include "util/filebuffer.hpp"

#ifndef MAGE_UTIL_BINARYFILE_HPP_
#define MAGE_UTIL_BINARYFILE_HPP_

namespace mage::util {
    class BinaryFileWriter : private BufferedFileWriter<false> {
    public:
        BinaryFileWriter(std::string output_file) : BufferedFileWriter<false>(output_file.c_str()), total_num_bits(0), current_byte(0) {
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

        void write32(std::uint32_t value) {
            for (int i = 0; i != 4; i++) {
                this->write8(static_cast<std::uint8_t>(value));
                value >>= 8;
            }
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
        BinaryFileReader(std::string input_file) : BufferedFileReader<false>(input_file.c_str()), total_num_bits(0), current_byte(0) {
        }

        std::uint8_t read1() {
            if ((this->total_num_bits & 0x7) == 0) {
                this->current_byte = this->start_read<std::uint8_t>();
                this->finish_read(sizeof(std::uint8_t));
            }
            std::uint8_t bit = this->current_byte & 0x1;
            this->current_byte >>= 1;
            this->total_num_bits++;
            return bit;
        }

    private:
        std::uint64_t total_num_bits;
        std::uint8_t current_byte;
    };
}

#endif
