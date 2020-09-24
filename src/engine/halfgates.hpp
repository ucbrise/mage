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

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include "crypto/block.hpp"
#include "engine/cluster.hpp"
#include "platform/network.hpp"
#include "schemes/halfgates.hpp"
#include "util/binaryfile.hpp"
#include "util/userpipe.hpp"

namespace mage::engine {
    const constexpr std::uint64_t halfgates_max_batch_size = 4 * crypto::block_num_bits;
    constexpr const std::size_t halfgates_num_connections = 2;

    template <typename T, std::size_t batch_size>
    class InputBatchPipe : public util::UserPipe<std::array<T, batch_size>> {
    public:
        InputBatchPipe(std::size_t capacity_shift)
            : util::UserPipe<std::array<T, batch_size>>(capacity_shift), index_into_batch(0) {
        }

        void read_elements(T* into, std::size_t count) {
            std::size_t read_so_far = 0;
            while (read_so_far != count) {
                std::array<T, batch_size>& latest = this->start_read_single_in_place();
                T* batch_data = latest.data();
                std::size_t to_read = std::min(batch_size - this->index_into_batch, count - read_so_far);
                std::copy(&batch_data[this->index_into_batch], &batch_data[this->index_into_batch + to_read], &into[read_so_far]);
                read_so_far += to_read;
                this->index_into_batch += to_read;
                if (this->index_into_batch == batch_size) {
                    this->index_into_batch = 0;
                    this->finish_read_single_in_place();
                }
            }
        }

    private:
        std::size_t index_into_batch;
    };

    class HalfGatesGarblingEngine {
    public:
        using Wire = schemes::HalfGatesGarbler::Wire;

        HalfGatesGarblingEngine(std::shared_ptr<ClusterNetwork>& network, const char* input_file, const char* output_file, const char* evaluator_host, const char* evaluator_port)
            : input_reader(input_file), output_writer(output_file), evaluator_input_labels(2) {
            platform::network_connect(evaluator_host, evaluator_port, this->sockets.data(), nullptr, halfgates_num_connections);
            this->conn_reader.set_file_descriptor(this->sockets[0], false);
            this->conn_writer.set_file_descriptor(this->sockets[0], false);
            this->ot_conn_reader.set_file_descriptor(this->sockets[1], false);
            this->ot_conn_writer.set_file_descriptor(this->sockets[1], false);

            this->start_input_daemon();

            this->conn_writer.enable_stats("GATE-SEND (ns)");
            crypto::block input_seed;
            if (network->get_self() == 0) {
                MessageChannel* c = network->contact_worker(1);
                Wire* buffer = c->write<Wire>(1);
                input_seed = this->garbler.initialize(*buffer);
                c->flush();
            } else {
                MessageChannel* c = network->contact_worker(0);
                Wire* buffer = c->read<Wire>(1);
                input_seed = this->garbler.initialize_with_delta(*buffer);
            }
            this->conn_writer.write<Wire>() = input_seed;
            this->conn_writer.flush();
        }

        ~HalfGatesGarblingEngine() {
            this->conn_writer.flush(); // otherwise we may deadlock
            for (std::uint64_t i = 0; i != this->output_label_lsbs.size(); i++) {
                std::uint8_t evaluator_lsb = this->conn_reader.read<std::uint8_t>();
                this->output_writer.write1(this->output_label_lsbs[i] ^ evaluator_lsb);
            }
            this->conn_reader.relinquish_file_descriptor();
            this->conn_writer.relinquish_file_descriptor();
            this->input_daemon.join();
            this->ot_conn_reader.relinquish_file_descriptor();
            this->ot_conn_writer.relinquish_file_descriptor();
            for (std::size_t i = 0; i != halfgates_num_connections; i++) {
                platform::network_close(this->sockets[i]);
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
                this->evaluator_input_labels.read_elements(data, length);
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
        void start_input_daemon();

        schemes::HalfGatesGarbler garbler;

        util::BinaryFileReader input_reader;
        util::BinaryFileWriter output_writer;
        std::array<int, halfgates_num_connections> sockets;
        util::BufferedFileReader<false> conn_reader;
        util::BufferedFileWriter<false> conn_writer;
        std::vector<std::uint8_t> output_label_lsbs;

        std::thread input_daemon;
        util::BufferedFileReader<false> ot_conn_reader;
        util::BufferedFileWriter<false> ot_conn_writer;
        InputBatchPipe<crypto::block, halfgates_max_batch_size> evaluator_input_labels;
    };

    class HalfGatesEvaluationEngine {
    public:
        using Wire = schemes::HalfGatesEvaluator::Wire;

        HalfGatesEvaluationEngine(const char* input_file, const char* evaluator_port)
            : input_reader(input_file), evaluator_input_labels(2) {
            platform::network_accept(evaluator_port, this->sockets.data(), halfgates_num_connections);
            this->conn_reader.set_file_descriptor(this->sockets[0], false);
            this->conn_writer.set_file_descriptor(this->sockets[0], false);
            this->ot_conn_reader.set_file_descriptor(this->sockets[1], false);
            this->ot_conn_writer.set_file_descriptor(this->sockets[1], false);

            this->start_input_daemon();

            this->conn_reader.enable_stats("GATE-RECV (ns)");
            crypto::block input_seed = this->conn_reader.read<crypto::block>();
            this->evaluator.initialize(input_seed);
        }

        ~HalfGatesEvaluationEngine() {
            this->conn_reader.relinquish_file_descriptor();
            this->conn_writer.relinquish_file_descriptor();
            this->input_daemon.join();
            this->ot_conn_reader.relinquish_file_descriptor();
            this->ot_conn_writer.relinquish_file_descriptor();
            for (std::size_t i = 0; i != halfgates_num_connections; i++) {
                platform::network_close(this->sockets[i]);
            }
        }

        void input(Wire* data, unsigned int length, bool garbler) {
            if (garbler) {
                this->evaluator.input_garbler(data, length);
            } else {
                this->evaluator_input_labels.read_elements(data, length);
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
        void start_input_daemon();

        schemes::HalfGatesEvaluator evaluator;
        util::BinaryFileReader input_reader;
        std::array<int, halfgates_num_connections> sockets;
        util::BufferedFileReader<false> conn_reader;
        util::BufferedFileWriter<false> conn_writer;

        /* For the input daemon. */
        std::thread input_daemon;
        util::BufferedFileReader<false> ot_conn_reader;
        util::BufferedFileWriter<false> ot_conn_writer;
        InputBatchPipe<crypto::block, halfgates_max_batch_size> evaluator_input_labels;
    };
}

#endif
