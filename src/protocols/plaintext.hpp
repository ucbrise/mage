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

#ifndef MAGE_PROTOCOLS_PLAINTEXT_HPP_
#define MAGE_PROTOCOLS_PLAINTEXT_HPP_

#include <iostream>
#include <string>
#include "util/binaryfile.hpp"

namespace mage::protocols::plaintext {
    class PlaintextEvaluationEngine {
    public:
        using Wire = unsigned __int128;

        PlaintextEvaluationEngine(std::string garbler_input_file, std::string evaluator_input_file, std::string output_file)
            : garbler_input_reader(garbler_input_file.c_str()), evaluator_input_reader(evaluator_input_file.c_str()), output_writer(output_file.c_str()) {
        }

        void print_stats() {
        }

        void input(Wire* data, unsigned int length, bool garbler) {
            util::BinaryFileReader& input_reader = garbler ? this->garbler_input_reader : this->evaluator_input_reader;
            for (unsigned int i = 0; i != length; i++) {
                std::uint8_t bit = input_reader.read1();
                data[i] = bit;
            }
        }

        void output(const Wire* data, unsigned int length) {
            for (unsigned int i = 0; i != length; i++) {
                std::uint8_t bit = static_cast<std::uint8_t>(data[i]) & 0x1;
                this->output_writer.write1(bit);
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
        util::BinaryFileReader garbler_input_reader;
        util::BinaryFileReader evaluator_input_reader;
        util::BinaryFileWriter output_writer;
    };
}

#endif
