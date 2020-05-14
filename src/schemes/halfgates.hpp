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

#ifndef MAGE_SCHEMES_HALFGATES_HPP_
#define MAGE_SCHEMES_HALFGATES_HPP_

#include <cstdint>
#include <string>
#include "util/binaryfile.hpp"

namespace mage::schemes {
    class HalfGatesGarbler {
    public:
        using Wire = unsigned __int128;

        HalfGatesGarbler(std::string input_file, std::string output_file, int conn_fd)
            : input_reader(input_file.c_str()), output_writer(output_file.c_str()), conn_reader(conn_fd), conn_writer(conn_fd), num_output_bits(0) {
            this->conn_writer.write<Wire>() = 0;
            this->conn_writer.flush();
        }

        ~HalfGatesGarbler() {
            for (std::uint64_t i = 0; i != this->num_output_bits; i++) {
                Wire result = this->conn_reader.read<Wire>();
                std::uint8_t output_bit = static_cast<std::uint8_t>(result) & 0x1;
                this->output_writer.write1(output_bit);
            }
        }

        void input(Wire* data, unsigned int length) {
            for (unsigned int i = 0; i != length; i++) {
                std::uint8_t bit = this->input_reader.read1();
                data[i] = bit;
            }
        }

        void output(const Wire* data, unsigned int length) {
            this->num_output_bits += length;
        }

        void op_and(Wire& output, const Wire& input1, const Wire& input2) {
            output = input1 & input2;
        }

        void op_xor(Wire& output, const Wire& input1, const Wire& input2) {
            output = input1 ^ input2;
        }

        void op_not(Wire& output, const Wire& input) {
            output = !input;
        }

        void op_xnor(Wire& output, const Wire& input1, const Wire& input2) {
            output = !(input1 ^ input2);
        }

        void op_copy(Wire& output, const Wire& input) {
            output = input;
        }

        void one(Wire& output) const {
            output = 1;
        }

        void zero(Wire& output) const {
            output = 0;
        }

    private:
        util::BinaryFileReader input_reader;
        util::BinaryFileWriter output_writer;
        util::BufferedFileReader<false> conn_reader;
        util::BufferedFileWriter<false> conn_writer;

        std::uint64_t num_output_bits;
    };

    class HalfGatesEvaluator {
    public:
        using Wire = unsigned __int128;

        HalfGatesEvaluator(std::string input_file, int conn_fd)
            : input_reader(input_file.c_str()), conn_reader(conn_fd), conn_writer(conn_fd) {
            this->seed = this->conn_reader.read<Wire>();
        }

        void input(Wire* data, unsigned int length) {
            for (unsigned int i = 0; i != length; i++) {
                std::uint8_t bit = this->input_reader.read1();
                data[i] = bit;
            }
        }

        void output(const Wire* data, unsigned int length) {
            for (unsigned int i = 0; i != length; i++) {
                this->conn_writer.write<Wire>() = data[i];
            }
        }

        void op_and(Wire& output, const Wire& input1, const Wire& input2) {
            output = input1 & input2;
        }

        void op_xor(Wire& output, const Wire& input1, const Wire& input2) {
            output = input1 ^ input2;
        }

        void op_not(Wire& output, const Wire& input) {
            output = !input;
        }

        void op_xnor(Wire& output, const Wire& input1, const Wire& input2) {
            output = !(input1 ^ input2);
        }

        void op_copy(Wire& output, const Wire& input) {
            output = input;
        }

        void one(Wire& output) const {
            output = 1;
        }

        void zero(Wire& output) const {
            output = 0;
        }

    private:
        util::BinaryFileReader input_reader;
        util::BufferedFileReader<false> conn_reader;
        util::BufferedFileWriter<false> conn_writer;

        Wire seed;
    };
}

#endif
