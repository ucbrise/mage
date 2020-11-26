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

#ifndef MAGE_PROTOCOLS_TFHE_HPP_
#define MAGE_PROTOCOLS_TFHE_HPP_

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include "engine/cluster.hpp"
#include "platform/network.hpp"
#include "protocols/tfhe_scheme.hpp"
#include "util/binaryfile.hpp"
#include "util/userpipe.hpp"

namespace mage::protocols::tfhe {
    class TFHEEngine {
    public:
        using Wire = TFHEScheme::Wire;

        TFHEEngine(const char* garbler_input_file, const char* evaluator_input_file, const char* output_file)
            : garbler_input_reader(garbler_input_file, std::ios::binary), evaluator_input_reader(evaluator_input_file, std::ios::binary), output_writer(output_file, std::ios::binary) {
            {
                std::ifstream params_file("params", std::ios::binary);
                if (!params_file.is_open()) {
                    std::cerr << "Could not open params" << std::endl;
                    std::abort();
                }
                this->tfhe.set_params(params_file);
            }
            {
                std::ifstream cloud_key_file("cloud.key", std::ios::binary);
                if (!cloud_key_file.is_open()) {
                    std::cerr << "Could not open cloud.key" << std::endl;
                    std::abort();
                }
                this->tfhe.set_cloud_key(cloud_key_file);
            }
        }

        void input(Wire* data, unsigned int length, bool garbler) {
            std::ifstream* reader_ptr = garbler ? &this->garbler_input_reader : &this->evaluator_input_reader;
            reader_ptr->read(reinterpret_cast<char*>(data), length * sizeof(Wire));
            if (reader_ptr->eof()) {
                std::cerr << "TFHE::input -> std::ifstream::read: end of file" << std::endl;
                std::abort();
            } else if (reader_ptr->fail() || reader_ptr->bad()) {
                std::cerr << "TFHE::input -> std::ifstream::read: failure" << std::endl;
                std::abort();
            }
        }

        void output(const Wire* data, unsigned int length) {
            this->output_writer.write(reinterpret_cast<const char*>(data), length * sizeof(Wire));
            if (this->output_writer.fail() || this->output_writer.bad()) {
                std::cerr << "TFHE::output -> std::ofstream::write: failure" << std::endl;
                std::abort();
            }
        }

        void op_and(Wire& output, const Wire& input1, const Wire& input2) {
            this->tfhe.op_and(output, input1, input2);
        }

        void op_xor(Wire& output, const Wire& input1, const Wire& input2) {
            this->tfhe.op_xor(output, input1, input2);
        }

        void op_not(Wire& output, const Wire& input) {
            this->tfhe.op_not(output, input);
        }

        void op_xnor(Wire& output, const Wire& input1, const Wire& input2) {
            this->tfhe.op_xnor(output, input1, input2);
        }

        void op_copy(Wire& output, const Wire& input) {
            this->tfhe.op_copy(output, input);
        }

        void one(Wire& output) {
            this->tfhe.one(output);
        }

        void zero(Wire& output) {
            this->tfhe.zero(output);
        }

    private:
        TFHEScheme tfhe;

        std::ifstream garbler_input_reader;
        std::ifstream evaluator_input_reader;
        std::ofstream output_writer;
    };
}

#endif
