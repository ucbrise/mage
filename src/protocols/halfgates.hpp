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

#ifndef MAGE_PROTOCOLS_HALFGATES_HPP_
#define MAGE_PROTOCOLS_HALFGATES_HPP_

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
#include "protocols/halfgates_scheme.hpp"
#include "util/binaryfile.hpp"
#include "util/userpipe.hpp"

namespace mage::protocols::halfgates {
    const constexpr std::uint64_t halfgates_max_batch_size = 4 * crypto::block_num_bits;
    constexpr const std::size_t halfgates_num_input_daemons = 3;
    constexpr const std::size_t halfgates_num_connections = 1 + halfgates_num_input_daemons;
    constexpr const std::size_t halfgates_output_batch_size = 1 << 21; // number of bits to buffer before completing a round and writing it to a file

    template <typename T, std::size_t batch_size>
    class InputBatchPipe : public util::UserPipe<std::array<T, batch_size>> {
    public:
        InputBatchPipe(std::size_t capacity_shift)
            : util::UserPipe<std::array<T, batch_size>>(capacity_shift), index_into_batch(0) {
        }

        std::pair<std::size_t, bool> read_elements_until_end_of_batch(T* into, std::size_t count) {
            std::array<T, batch_size>& latest = *(this->start_read_single_in_place());
            T* batch_data = latest.data();
            std::size_t to_read = std::min(batch_size - this->index_into_batch, count);
            std::copy(&batch_data[this->index_into_batch], &batch_data[this->index_into_batch + to_read], into);
            this->index_into_batch += to_read;
            if (this->index_into_batch == batch_size) {
                this->index_into_batch = 0;
                this->finish_read_single_in_place();
                return std::make_pair(to_read, true);
            }
            return std::make_pair(to_read, false);
        }

        void read_elements(T* into, std::size_t count) {
            std::size_t read_so_far = 0;
            while (read_so_far != count) {
                auto [ bytes_read, end_of_batch ] = this->read_elements_until_end_of_batch(&into[read_so_far], count - read_so_far);
                read_so_far += bytes_read;
            }
        }

    private:
        std::size_t index_into_batch;
    };

    struct InputDaemonThread {
        InputDaemonThread() : evaluator_input_labels(2) {
        }

        std::thread thread;
        util::BufferedFileReader<false> ot_conn_reader;
        util::BufferedFileWriter<false> ot_conn_writer;
        InputBatchPipe<crypto::block, halfgates_max_batch_size> evaluator_input_labels;
    };

    class HalfGatesGarblingEngine {
    public:
        using Wire = HalfGatesGarbler::Wire;

        HalfGatesGarblingEngine(const std::shared_ptr<engine::ClusterNetwork>& network, const char* input_file, const char* output_file, const char* evaluator_host, const char* evaluator_port)
            : input_reader(input_file), output_writer(output_file), conn_output_reader(this->conn_reader), input_daemon_threads(halfgates_num_input_daemons), evaluator_input_index(0) {
            platform::network_connect(evaluator_host, evaluator_port, this->sockets.data(), nullptr, halfgates_num_connections);
            this->conn_reader.set_file_descriptor(this->sockets[0], false);
            this->conn_writer.set_file_descriptor(this->sockets[0], false);
            for (int i = 0; i != halfgates_num_input_daemons; i++) {
                this->input_daemon_threads[i].ot_conn_reader.set_file_descriptor(this->sockets[1 + i], false);
                this->input_daemon_threads[i].ot_conn_writer.set_file_descriptor(this->sockets[1 + i], false);
            }

            this->conn_writer.enable_stats("GATE-SEND (ns)");
            crypto::block input_seed;
            if (network->get_self() == 0) {
                WorkerID num_workers = network->get_num_workers();
                Wire delta_precursor;
                input_seed = this->garbler.initialize(delta_precursor);
                for (WorkerID i = 1; i != num_workers; i++) {
                    engine::MessageChannel* c = network->contact_worker(i);
                    *(c->write<Wire>(1)) = delta_precursor;
                    c->flush();
                }
            } else {
                engine::MessageChannel* c = network->contact_worker(0);
                Wire buffer;
                c->read<Wire>(&buffer, 1);
                input_seed = this->garbler.initialize_with_delta(buffer);
            }
            this->conn_writer.write<Wire>() = input_seed;
            this->conn_writer.flush();

            /* Once this->garbler is initialized, start the OT daemon. */
            this->start_input_daemon();
        }

        ~HalfGatesGarblingEngine() {
            this->write_pending_output_data();
            this->conn_reader.relinquish_file_descriptor();
            this->conn_writer.relinquish_file_descriptor();
            for (int i = 0; i != halfgates_num_input_daemons; i++) {
                this->input_daemon_threads[i].thread.join();
                this->input_daemon_threads[i].ot_conn_reader.relinquish_file_descriptor();
                this->input_daemon_threads[i].ot_conn_writer.relinquish_file_descriptor();
            }
            for (std::size_t i = 0; i != halfgates_num_connections; i++) {
                platform::network_close(this->sockets[i]);
            }
        }

        void print_stats() {
            std::cout << this->conn_writer.get_stats() << std::endl;
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
                std::size_t read_so_far = 0;
                while (read_so_far != length) {
                    auto& label_pipe = this->input_daemon_threads[this->evaluator_input_index].evaluator_input_labels;
                    auto [ bytes_read, end_of_batch ] = label_pipe.read_elements_until_end_of_batch(&data[read_so_far], length - read_so_far);
                    read_so_far += bytes_read;
                    if (end_of_batch) {
                        this->evaluator_input_index++;
                        if (this->evaluator_input_index == halfgates_num_input_daemons) {
                            this->evaluator_input_index = 0;
                        }
                    }
                }
            }
        }

        void write_pending_output_data() {
            this->conn_writer.flush(); // otherwise we may deadlock
            for (std::uint64_t i = 0; i != this->output_label_lsbs.size(); i++) {
                bool evaluator_lsb = this->conn_output_reader.read1();
                this->output_writer.write1((this->output_label_lsbs[i] == evaluator_lsb) ? 0x0 : 0x1);
            }
        }

        // HACK: assume all output goes to the garbler
        void output(const Wire* data, unsigned int length) {
            while (length != 0) {
                unsigned int length_to_process = std::min(static_cast<unsigned int>(halfgates_output_batch_size - this->output_label_lsbs.size()), length);
                this->garbler.output(&this->output_label_lsbs, data, length_to_process);
                data += length_to_process;
                length -= length_to_process;
                if (this->output_label_lsbs.size() == halfgates_output_batch_size) {
                    this->write_pending_output_data();
                    this->output_label_lsbs.clear();
                }
            }
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

        HalfGatesGarbler garbler;

        util::BinaryFileReader input_reader;
        util::BinaryFileWriter output_writer;
        std::array<int, halfgates_num_connections> sockets;
        util::BufferedFileReader<false> conn_reader;
        util::BufferedFileWriter<false> conn_writer;
        util::BinaryReader conn_output_reader;
        std::vector<bool> output_label_lsbs;

        std::vector<InputDaemonThread> input_daemon_threads;
        std::size_t evaluator_input_index;
    };

    class HalfGatesEvaluationEngine {
    public:
        using Wire = HalfGatesEvaluator::Wire;

        HalfGatesEvaluationEngine(const char* input_file, const char* evaluator_port)
            : input_reader(input_file), conn_output_writer(this->conn_writer), input_daemon_threads(halfgates_num_input_daemons), evaluator_input_index(0),
            bits_left_in_output_batch(halfgates_output_batch_size) {
            platform::network_accept(evaluator_port, this->sockets.data(), halfgates_num_connections);
            this->conn_reader.set_file_descriptor(this->sockets[0], false);
            this->conn_writer.set_file_descriptor(this->sockets[0], false);
            for (int i = 0; i != halfgates_num_input_daemons; i++) {
                this->input_daemon_threads[i].ot_conn_reader.set_file_descriptor(this->sockets[1 + i], false);
                this->input_daemon_threads[i].ot_conn_writer.set_file_descriptor(this->sockets[1 + i], false);
            }

            this->conn_reader.enable_stats("GATE-RECV (ns)");
            crypto::block input_seed = this->conn_reader.read<crypto::block>();
            this->evaluator.initialize(input_seed);

            /* Once this->evaluator is initialized, start the OT daemon. */
            this->start_input_daemon();
        }

        ~HalfGatesEvaluationEngine() {
            this->conn_reader.relinquish_file_descriptor();
            this->conn_writer.relinquish_file_descriptor();
            for (int i = 0; i != halfgates_num_input_daemons; i++) {
                this->input_daemon_threads[i].thread.join();
                this->input_daemon_threads[i].ot_conn_reader.relinquish_file_descriptor();
                this->input_daemon_threads[i].ot_conn_writer.relinquish_file_descriptor();
            }
            for (std::size_t i = 0; i != halfgates_num_connections; i++) {
                platform::network_close(this->sockets[i]);
            }
        }

        void print_stats() {
            std::cout << this->conn_reader.get_stats() << std::endl;
        }

        void input(Wire* data, unsigned int length, bool garbler) {
            if (garbler) {
                this->evaluator.input_garbler(data, length);
            } else {
                std::size_t read_so_far = 0;
                while (read_so_far != length) {
                    auto& label_pipe = this->input_daemon_threads[this->evaluator_input_index].evaluator_input_labels;
                    auto [ bytes_read, end_of_batch ] = label_pipe.read_elements_until_end_of_batch(&data[read_so_far], length - read_so_far);
                    read_so_far += bytes_read;
                    if (end_of_batch) {
                        this->evaluator_input_index++;
                        if (this->evaluator_input_index == halfgates_num_input_daemons) {
                            this->evaluator_input_index = 0;
                        }
                    }
                }
            }
        }

        void output(const Wire* data, unsigned int length) {
            while (length != 0) {
                unsigned int to_write_this_iteration = std::min(static_cast<unsigned int>(this->bits_left_in_output_batch), length);
                this->evaluator.output(&this->conn_output_writer, data, length);
                length -= to_write_this_iteration;
                data += to_write_this_iteration;
                this->bits_left_in_output_batch -= length;
                if (this->bits_left_in_output_batch == 0) {
                    this->conn_writer.flush();
                    this->bits_left_in_output_batch = halfgates_output_batch_size;
                }
            }
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

        HalfGatesEvaluator evaluator;
        util::BinaryFileReader input_reader;
        std::array<int, halfgates_num_connections> sockets;
        util::BufferedFileReader<false> conn_reader;
        util::BufferedFileWriter<false> conn_writer;
        util::BinaryWriter conn_output_writer;

        /* For the input daemon. */
        std::vector<InputDaemonThread> input_daemon_threads;
        std::size_t evaluator_input_index;
        std::size_t bits_left_in_output_batch;
    };
}

#endif
