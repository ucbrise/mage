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
 * @file util/binaryfile.hpp
 * @brief Tools for writing data to files in binary form.
 */

#ifndef MAGE_UTIL_BINARYFILE_HPP_
#define MAGE_UTIL_BINARYFILE_HPP_

#include <cstdint>
#include <algorithm>
#include "platform/filesystem.hpp"
#include "util/filebuffer.hpp"

namespace mage::util {
    /**
     * @brief Writes binary data to a file, decomposing structured data (e.g.,
     * integers and floating point numbers) into bits.
     *
     * Normally, a file is understood as a sequence of bytes, but this class
     * allows one to think of a file as a sequence of bits. This is implemented
     * by filling bits into each byte from least significant bit to most
     * significant bit. The underlying BufferedFileWriter can be flushed at
     * any time without affecting correctness of the BinaryWriter.
     */
    class BinaryWriter {
    public:
        /**
         * @brief Create a BinaryWriter.
         */
        BinaryWriter(BufferedFileWriter<false>& output_writer) : output(output_writer), total_num_bits(0), current_byte(0) {
        }

        /**
         * @brief A BinaryWriter is not copy-constructible.
         */
        BinaryWriter(BinaryWriter& other) = delete;

        /**
         * @brief If the last byte is incomplete, pads it with zeros to form a
         * full byte and writes it out.
         */
        ~BinaryWriter() {
            if ((static_cast<std::uint8_t>(this->total_num_bits) & 0x7) != 0) {
                this->output.write<std::uint8_t>() = this->current_byte;
            }
        }

        /**
         * @brief Writes a 64-bit integer to the file in order from least
         * significant bit to most significant bit.
         *
         * @param value The value whose bits to write to the file.
         */
        void write64(std::uint64_t value) {
            for (int i = 0; i != 8; i++) {
                this->write8(static_cast<std::uint8_t>(value));
                value >>= 8;
            }
        }

        /**
         * @brief Writes a 64-bit floating point number to the file by
         * interpreting its bits as if it were a 64-bit integer and writing
         * them out in order from least significant bit to most significant
         * bit.
         *
         * @param value The value whose bits to write to the file.
         */
        void write_double(double value) {
            std::uint64_t* ptr = reinterpret_cast<std::uint64_t*>(&value);
            this->write64(*ptr);
        }

        /**
         * @brief Writes a 32-bit integer to the file in order from least
         * significant bit to most significant bit.
         *
         * @param value The value whose bits to write to the file.
         */
        void write32(std::uint32_t value) {
            for (int i = 0; i != 4; i++) {
                this->write8(static_cast<std::uint8_t>(value));
                value >>= 8;
            }
        }

        /**
         * @brief Writes a 32-bit floating point number to the file by
         * interpreting its bits as if it were a 32-bit integer and writing
         * them out in order from least significant bit to most significant
         * bit.
         *
         * @param value The value whose bits to write to the file.
         */
        void write_float(float value) {
            std::uint32_t* ptr = reinterpret_cast<std::uint32_t*>(&value);
            this->write32(*ptr);
        }

        /**
         * @brief Writes a 16-bit integer to the file in order from least
         * significant bit to most significant bit.
         *
         * @param value The value whose bits to write to the file.
         */
        void write16(std::uint16_t value) {
            for (int i = 0; i != 2; i++) {
                this->write8(static_cast<std::uint8_t>(value));
                value >>= 8;
            }
        }

        /**
         * @brief Writes an 8-bit integer to the file in order from least
         * significant bit to most significant bit.
         *
         * @param byte The value whose bits to write to the file.
         */
        void write8(std::uint8_t byte) {
            std::uint8_t current_bit = static_cast<std::uint8_t>(this->total_num_bits) & 0x7;
            this->current_byte |= (byte << current_bit);
            this->output.write<std::uint8_t>() = this->current_byte;
            this->current_byte = (byte >> (8 - current_bit));
            this->total_num_bits += 8;
        }

        /**
         * @brief Writes a bit to the file.
         *
         * @param bit The least significant bit of this 8-bit integer is the
         * value of the bit written to the file.
         */
        void write1(std::uint8_t bit) {
            std::uint8_t current_bit = static_cast<std::uint8_t>(this->total_num_bits) & 0x7;
            this->current_byte |= (bit << current_bit);
            this->total_num_bits++;
            if (current_bit == 7) {
                this->output.write<std::uint8_t>() = this->current_byte;
                this->current_byte = 0;
            }
        }

    private:
        std::uint64_t total_num_bits;
        std::uint8_t current_byte;
        BufferedFileWriter<false>& output;
    };

    /**
     * @brief Easier-to-use wrapper for BinaryWriter that owns the underlying
     * BufferedFileWriter and creates a file into which to write the output.
     */
    class BinaryFileWriter : private BufferedFileWriter<false>, public BinaryWriter {
    public:
        /**
         * @brief Creates a file with the specified name and creates a
         * BinaryWriter to write binary data to it.
         *
         * @param output_file The name of the file to create.
         */
        BinaryFileWriter(const char* output_file) : BufferedFileWriter<false>(output_file), BinaryWriter(*static_cast<BufferedFileWriter*>(this)) {
        }
    };

    /**
     * @brief Reads binary data from a file, assembling bits into structured
     * data.
     *
     * Normally, a file is understood as a sequence of bytes, but this class
     * allows one to think of a file as a sequence of bits. This is implemented
     * by filling bits into each byte from least significant bit to most
     * significant bit. The underlying BufferedFileReader can be rebuffered at
     * any time without affecting correctness of the BinaryReader.
     */
    class BinaryReader {
    public:
        /**
         * @brief Create a BinaryReader.
         */
        BinaryReader(BufferedFileReader<false>& input_reader) : input(input_reader), current_bit(0), current_byte(0) {
        }

        /**
         * @brief A BinaryReader is not copy-constructible.
         */
        BinaryReader(BinaryReader& other) = delete;

        /**
         * @brief Read the next bit from the file.
         *
         * @return Either 0 or 1 depending on the bit's value.
         */
        std::uint8_t read1() {
            if (this->current_bit == 0) {
                this->current_byte = this->read<std::uint8_t>();
            }
            std::uint8_t bit = (this->current_byte >> this->current_bit) & 0x1;
            this->current_bit = (this->current_bit + 1) & 0x7;
            return bit;
        }

        /**
         * @brief Read an object of type @p T from the file by filling in its
         * bytes from first to last, and the bits in each byte from least
         * significant to most significant.
         *
         * The object of type @p T is initialized directly using the contents
         * of the file, so it should be a plain data type with the default
         * copy constructor and destructor. For example, it can safely be
         * taken to be an integer type.
         *
         * @tparam T The type of the object to read and initialize.
         * @return The object of type @p T initialized from the file data.
         */
        template <typename T>
        T read() {
            T rv;
            std::uint8_t* ptr = reinterpret_cast<std::uint8_t*>(&rv);
            this->read_bytes(ptr, sizeof(T));
            return rv;
        }

        /**
         * @brief Read an array of bytes from the file by filling in the bytes
         * from first to last, and the bits in each byte from least significant
         * to most significant.
         *
         * @param bytes Pointer to the array into which to read the bytes.
         * @param num_bytes The length of the array, in bytes.
         */
        void read_bytes(std::uint8_t* bytes, std::size_t num_bytes) {
            if (this->current_bit == 0) {
                void* from = this->input.start_read(num_bytes);
                std::uint8_t* from_bytes = static_cast<std::uint8_t*>(from);
                std::copy(from_bytes, from_bytes + num_bytes, bytes);
                this->input.finish_read(num_bytes);
            } else {
                for (std::uint32_t i = 0; i != num_bytes; i++) {
                    bytes[i] = (this->current_byte >> this->current_bit);
                    this->current_byte = this->input.read<std::uint8_t>();
                    bytes[i] |= (this->current_byte << (8 - this->current_bit));
                }
            }
        }

        /**
         * @brief Read an array of bits from the file by filling in the bytes
         * from first to last, and the bits in each byte from least significant
         * to most significant.
         *
         * If the number of bits is not a multiple of 8, the most significant
         * bits of the final byte, which represent bits beyond the number of
         * bits requested, are not guaranteed to be initialized in any
         * particular way. Values may be stored in those bits, but the user of
         * this function should not assume that they convey any particular
         * meaning.
         *
         * @param bytes Pointer to the array into which to read the bytes.
         * @param num_bytes The length of the array, in bytes.
         */
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
                    this->current_byte = this->input.read<std::uint8_t>();
                }
                bytes[num_bytes] = (this->current_byte >> this->current_bit);
                if (leftover_bits > (8 - this->current_bit)) {
                    this->current_byte = this->input.read<std::uint8_t>();
                    bytes[num_bytes] |= (this->current_byte << (8 - this->current_bit));
                }
                this->current_bit = (this->current_bit + leftover_bits) & 0x7;
            }
        }

    private:
        std::uint8_t current_bit;
        std::uint8_t current_byte;
        BufferedFileReader<false>& input;
    };

    /**
     * @brief Easier-to-use wrapper for BinaryWriter that owns the underlying
     * BufferedFileWriter and creates it by opening a file.
     */
    class BinaryFileReader : private BufferedFileReader<false>, public BinaryReader {
    public:
        /**
         * @brief Opens the file with the specified file name and creates a
         * BinaryReader that reads binary data from that file.
         */
        BinaryFileReader(const char* input_file, std::size_t buffer_size = 1 << 18) : BufferedFileReader<false>(input_file, buffer_size), BinaryReader(*static_cast<BufferedFileReader*>(this)) {
        }

        /**
         * @brief Gets the length of the underlying file, in bytes.
         *
         * @return The length of the underlying file, in bytes.
         */
        std::uint64_t get_file_length() const {
            return platform::length_file(this->fd);
        }
    };
}

#endif
