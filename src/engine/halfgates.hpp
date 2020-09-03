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

#ifndef MAGE_ENGINE_HALFGATES_HPP_
#define MAGE_ENGINE_HALFGATES_HPP_

#include <cstdint>
#include <string>
#include "crypto/block.hpp"
#include "crypto/ot/extension.hpp"
#include "schemes/halfgates.hpp"
#include "util/binaryfile.hpp"

namespace mage::engine {
    class HalfGatesGarblingEngine {
    public:
        using Wire = schemes::HalfGatesGarbler::Wire;

        HalfGatesGarblingEngine(std::string input_file, std::string output_file, int conn_fd)
            : input_reader(input_file.c_str()), output_writer(output_file.c_str()), conn_reader(conn_fd), conn_writer(conn_fd) {
            this->conn_writer.enable_stats("GATE-SEND (ns)");
            crypto::block input_seed = this->garbler.initialize();
            this->conn_writer.write<Wire>() = input_seed;
            this->conn_writer.flush();

            this->ot_sender.initialize(this->conn_reader, this->conn_writer);
        }

        ~HalfGatesGarblingEngine() {
            this->conn_writer.flush(); // otherwise we may deadlock
            for (std::uint64_t i = 0; i != this->output_label_lsbs.size(); i++) {
                std::uint8_t evaluator_lsb = this->conn_reader.read<std::uint8_t>();
                this->output_writer.write1(this->output_label_lsbs[i] ^ evaluator_lsb);
            }
        }

        void input(Wire* data, unsigned int length, bool garbler) {
            if (garbler) {
                bool input_bits[length];
                for (unsigned int i = 0; i != length; i++) {
                    std::uint8_t bit = this->input_reader.read1();
                    input_bits[i] = (bit != 0x0);
                }
                this->garbler.input_garbler(data, length, input_bits);
            } else {
                std::pair<Wire, Wire> ot_pairs[length];
                this->garbler.input_evaluator(data, length, ot_pairs);
                // crypto::ot::base_send(this->ot_group, this->conn_reader, this->conn_writer, ot_pairs, length);
                this->ot_sender.send(this->conn_reader, this->conn_writer, ot_pairs, length);
            }
        }

        // HACK: assume all output goes to the garbler
        void output(const Wire* data, unsigned int length) {
            std::size_t current_length = this->output_label_lsbs.size();
            this->output_label_lsbs.resize(current_length + length);
            this->garbler.output(&this->output_label_lsbs.data()[current_length], data, length);
        }

        void op_and(Wire& output, const Wire& input1, const Wire& input2) {
            crypto::block* table = reinterpret_cast<crypto::block*>(this->conn_writer.start_write(2 * sizeof(Wire)));
            this->garbler.op_and(table, output, input1, input2);
            this->conn_writer.finish_write(2 * sizeof(Wire));
        }

        void op_xor(Wire& output, const Wire& input1, const Wire& input2) {
            this->garbler.op_xor(output, input1, input2);
        }

        void op_not(Wire& output, const Wire& input) {
            this->garbler.op_not(output, input);
        }

        void op_xnor(Wire& output, const Wire& input1, const Wire& input2) {
            this->garbler.op_xnor(output, input1, input2);
        }

        void op_copy(Wire& output, const Wire& input) {
            this->garbler.op_copy(output, input);
        }

        void one(Wire& output) const {
            this->garbler.one(output);
        }

        void zero(Wire& output) const {
            this->garbler.zero(output);
        }

    private:
        schemes::HalfGatesGarbler garbler;

        util::BinaryFileReader input_reader;
        util::BinaryFileWriter output_writer;
        util::BufferedFileReader<false> conn_reader;
        util::BufferedFileWriter<false> conn_writer;
        std::vector<std::uint8_t> output_label_lsbs;

        // crypto::DDHGroup ot_group;
        crypto::ot::ExtensionSender ot_sender;
    };

    class HalfGatesEvaluationEngine {
    public:
        using Wire = schemes::HalfGatesEvaluator::Wire;

        HalfGatesEvaluationEngine(std::string input_file, int conn_fd)
            : input_reader(input_file.c_str()), conn_reader(conn_fd), conn_writer(conn_fd) {
            this->conn_reader.enable_stats("GATE-RECV (ns)");

            crypto::block input_seed = this->conn_reader.read<crypto::block>();
            this->evaluator.initialize(input_seed);

            this->ot_chooser.initialize(this->conn_reader, this->conn_writer);
        }

        void input(Wire* data, unsigned int length, bool garbler) {
            if (garbler) {
                this->evaluator.input_garbler(data, length);
            } else {
                /* Use OT to get label corresponding to bit. */
                // bool* choices = new bool[length];
                // for (unsigned int i = 0; i != length; i++) {
                //     std::uint8_t bit = this->input_reader.read1();
                //     choices[i] = (bit != 0);
                // }
                // crypto::ot::base_choose(this->ot_group, this->conn_reader, this->conn_writer, choices, data, length);
                std::size_t num_blocks = (length + crypto::block_num_bits - 1) / crypto::block_num_bits;
                crypto::block choices[num_blocks];
                choices[num_blocks - 1] = crypto::zero_block();
                this->input_reader.read_bits(reinterpret_cast<std::uint8_t*>(&choices[0]), length);
                // for (std::size_t i = 0; i != num_blocks; i++) { // this abomination works
                //     choices[i] = crypto::zero_block();
                //     unsigned __int128* x = reinterpret_cast<unsigned __int128*>(&choices[i]);
                //     for (std::size_t j = 0; j != crypto::block_num_bits && i * crypto::block_num_bits + j != length; j++) {
                //         std::uint8_t bit = this->input_reader.read1();
                //         *x |= (((unsigned __int128) bit) << j);
                //     }
                // }
                this->ot_chooser.choose(this->conn_reader, this->conn_writer, choices, data, length);
                // for (unsigned int i = 0; i != length; i++) {
                //     std::cout << "Asked for choice " << choices[i] << ", got " << *reinterpret_cast<std::uint64_t*>(&data[i]) << std::endl;
                // }
                // delete[] choices;
            }
        }

        void output(const Wire* data, unsigned int length) {
            std::uint8_t* into = reinterpret_cast<std::uint8_t*>(this->conn_writer.start_write(length));
            this->evaluator.output(into, data, length);
            this->conn_writer.finish_write(length);
        }

        void op_and(Wire& output, const Wire& input1, const Wire& input2) {
            crypto::block* table = reinterpret_cast<crypto::block*>(this->conn_reader.start_read(2 * sizeof(crypto::block)));
            this->evaluator.op_and(table, output, input1, input2);
            this->conn_reader.finish_read(2 * sizeof(crypto::block));
        }

        void op_xor(Wire& output, const Wire& input1, const Wire& input2) {
            this->evaluator.op_xor(output, input1, input2);
        }

        void op_not(Wire& output, const Wire& input) {
            this->evaluator.op_not(output, input);
        }

        void op_xnor(Wire& output, const Wire& input1, const Wire& input2) {
            this->evaluator.op_xnor(output, input1, input2);
        }

        void op_copy(Wire& output, const Wire& input) {
            this->evaluator.op_copy(output, input);
        }

        void one(Wire& output) const {
            this->evaluator.one(output);
        }

        void zero(Wire& output) const {
            this->evaluator.zero(output);
        }

    private:
        schemes::HalfGatesEvaluator evaluator;
        util::BinaryFileReader input_reader;
        util::BufferedFileReader<false> conn_reader;
        util::BufferedFileWriter<false> conn_writer;

        // crypto::DDHGroup ot_group;
        crypto::ot::ExtensionChooser ot_chooser;
    };
}

#endif
