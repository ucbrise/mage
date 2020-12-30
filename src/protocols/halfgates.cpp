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

#include "protocols/halfgates.hpp"
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include "addr.hpp"
#include "crypto/block.hpp"
#include "crypto/ot/correlated.hpp"
#include "engine/andxor.hpp"
#include "memprog/program.hpp"
#include "protocols/registry.hpp"
#include "util/filebuffer.hpp"
#include "util/misc.hpp"

namespace mage::protocols::halfgates {
    struct LockedCounter {
        std::uint64_t count;
        std::mutex mutex;
    };

    void HalfGatesGarblingEngine::start_input_daemon(std::size_t max_batch_size, std::size_t pipeline_depth) {
        std::uint64_t num_bytes = this->input_daemon_threads[0]->ot_conn_reader.read<std::uint64_t>();
        std::uint64_t num_bits = num_bytes << 3;
        std::int64_t total_batches, extra_bits;
        std::tie(total_batches, extra_bits) = util::floor_div(num_bits, max_batch_size);
        if ((max_batch_size & 0x7) != 0) {
            std::cerr << "Invalid batch size" << std::endl;
            std::abort();
        }
        assert((num_bits & 0x7) == 0);

        for (int i = 0; i != this->input_daemon_threads.size(); i++) {
            InputDaemonThread* daemon = this->input_daemon_threads[i].get();
            daemon->thread = std::thread([=]() {
                auto [ num_batches, num_extra_batches ] = util::floor_div(total_batches, this->input_daemon_threads.size());
                if (i < num_extra_batches) {
                    num_batches++;
                }
                if (i == num_extra_batches && extra_bits != 0) {
                    num_batches++;
                    // This thread will handle the smaller extra batch
                }

                // crypto::ot::CorrelatedExtensionSender ot_sender;
                // ot_sender.initialize(daemon->ot_conn_reader, daemon->ot_conn_writer);
                crypto::ot::PipelinedCorrelatedExtSender ot_sender(daemon->ot_conn_reader, daemon->ot_conn_writer, this->garbler.get_delta(), daemon->evaluator_input_labels, max_batch_size, pipeline_depth);

                for (std::int64_t c = 0; c != num_batches; c++) {
                    std::uint64_t batch_size = max_batch_size;
                    if (c == num_batches - 1 && i == num_extra_batches && extra_bits != 0) {
                        batch_size = extra_bits;
                    }
                    assert((batch_size & 0x7) == 0);

                    // std::pair<crypto::block, crypto::block>* ot_pairs = new std::pair<crypto::block, crypto::block>[batch_size];
                    // {
                    //     Wire* batch = new Wire[batch_size];
                    //     this->garbler.input_evaluator(batch, batch_size, ot_pairs);
                    //     this->evaluator_input_labels.write_contiguous(batch, batch_size);
                    //     delete[] batch;
                    // }
                    // ot_sender.send(this->ot_conn_reader, this->ot_conn_writer, ot_pairs, batch_size);
                    // delete[] ot_pairs;

                    // crypto::block* batch = daemon->evaluator_input_labels.start_write_in_place(halfgates_max_batch_size);
                    // ot_sender.send(daemon->ot_conn_reader, daemon->ot_conn_writer, this->garbler.get_delta(), batch, batch_size);
                    // daemon->evaluator_input_labels.finish_write_in_place(halfgates_max_batch_size);

                    ot_sender.submit_send();
                }
            });
        }
        // this->input_daemon = std::thread([=]() {
        //     crypto::ot::CorrelatedExtensionSender ot_sender;
        //     std::uint64_t num_bytes = this->ot_conn_reader.read<std::uint64_t>();
        //
        //     ot_sender.initialize(this->ot_conn_reader, this->ot_conn_writer);
        //
        //     std::uint64_t num_bits = num_bytes << 3;
        //     static_assert((halfgates_max_batch_size & 0x7) == 0);
        //     assert((num_bits & 0x7) == 0);
        //     for (std::uint64_t i = 0; i < num_bits; i += halfgates_max_batch_size) {
        //         std::uint64_t batch_size = std::min(halfgates_max_batch_size, num_bits - i);
        //         // std::pair<crypto::block, crypto::block>* ot_pairs = new std::pair<crypto::block, crypto::block>[batch_size];
        //         // {
        //         //     Wire* batch = new Wire[batch_size];
        //         //     this->garbler.input_evaluator(batch, batch_size, ot_pairs);
        //         //     this->evaluator_input_labels.write_contiguous(batch, batch_size);
        //         //     delete[] batch;
        //         // }
        //         // ot_sender.send(this->ot_conn_reader, this->ot_conn_writer, ot_pairs, batch_size);
        //         // delete[] ot_pairs;
        //
        //         std::array<crypto::block, halfgates_max_batch_size>& batch = *(this->evaluator_input_labels.start_write_single_in_place());
        //         ot_sender.send(this->ot_conn_reader, this->ot_conn_writer, this->garbler.get_delta(), batch.data(), batch_size);
        //         this->evaluator_input_labels.finish_write_single_in_place();
        //     }
        // });
    }

    struct InputTurn {
        InputTurn(std::size_t num_input_daemons) : thread_turn_active(num_input_daemons) {
        }

        int turn;
        std::mutex turn_mutex;
        std::vector<std::condition_variable> thread_turn_active;
    };

    void HalfGatesEvaluationEngine::start_input_daemon(std::size_t max_batch_size, std::size_t pipeline_depth) {
        std::uint64_t num_bytes = this->input_reader.get_file_length();
        this->input_daemon_threads[0]->ot_conn_writer.write<std::uint64_t>() = num_bytes;
        this->input_daemon_threads[0]->ot_conn_writer.flush();
        std::uint64_t num_bits = num_bytes << 3;
        std::int64_t total_batches, extra_bits;
        std::tie(total_batches, extra_bits) = util::floor_div(num_bits, max_batch_size);
        if ((max_batch_size & 0x7) != 0) {
            std::cerr << "Invalid batch size" << std::endl;
            std::abort();
        }
        assert((num_bits & 0x7) == 0);

        std::shared_ptr<InputTurn> turn_info = std::make_shared<InputTurn>(this->input_daemon_threads.size());
        turn_info->turn = 0;

        for (int i = 0; i != this->input_daemon_threads.size(); i++) {
            InputDaemonThread* daemon = this->input_daemon_threads[i].get();
            daemon->thread = std::thread([=]() {
                int next_thread_i = (i + 1) % this->input_daemon_threads.size();
                auto [ num_batches, num_extra_batches ] = util::floor_div(total_batches, this->input_daemon_threads.size());
                if (i < num_extra_batches) {
                    num_batches++;
                }
                if (i == num_extra_batches && extra_bits != 0) {
                    num_batches++;
                    // This thread will handle the smaller extra batch
                }

                // crypto::ot::CorrelatedExtensionChooser ot_chooser;
                // ot_chooser.initialize(daemon->ot_conn_reader, daemon->ot_conn_writer);
                crypto::ot::PipelinedCorrelatedExtChooser ot_chooser(daemon->ot_conn_reader, daemon->ot_conn_writer, daemon->evaluator_input_labels, max_batch_size, pipeline_depth);

                for (std::int64_t c = 0; c != num_batches; c++) {
                    std::uint64_t batch_size = max_batch_size;
                    if (c == num_batches - 1 && i == num_extra_batches && extra_bits != 0) {
                        batch_size = extra_bits;
                    }
                    assert((batch_size & 0x7) == 0);
                    std::int64_t num_blocks = util::ceil_div(batch_size, crypto::block_num_bits).first;

                    crypto::block choices[num_blocks];
                    choices[num_blocks - 1] = crypto::zero_block();
                    {
                        std::unique_lock<std::mutex> lock(turn_info->turn_mutex);
                        while (turn_info->turn != i) {
                            turn_info->thread_turn_active[i].wait(lock);
                        }
                        this->input_reader.read_bytes(reinterpret_cast<std::uint8_t*>(&choices[0]), batch_size >> 3);
                        turn_info->turn = next_thread_i;
                        turn_info->thread_turn_active[next_thread_i].notify_one();
                    }

                    // std::array<crypto::block, halfgates_max_batch_size> batch;
                    // ot_chooser.choose(daemon->ot_conn_reader, daemon->ot_conn_writer, choices, batch.data(), batch_size);
                    // this->evaluator_input_labels.write_contiguous(&batch, 1);

                    // crypto::block* batch = daemon->evaluator_input_labels.start_write_in_place(halfgates_max_batch_size);
                    // ot_chooser.choose(daemon->ot_conn_reader, daemon->ot_conn_writer, choices, batch, batch_size);
                    // daemon->evaluator_input_labels.finish_write_in_place(halfgates_max_batch_size);

                    ot_chooser.submit_choose(choices);
                }
            });
        // this->input_daemon = std::thread([=]() {
        //     crypto::ot::CorrelatedExtensionChooser ot_chooser;
        //     std::uint64_t num_bytes = this->input_reader.get_file_length();
        //     this->ot_conn_writer.write<std::uint64_t>() = num_bytes;
        //     this->ot_conn_writer.flush();
        //
        //     ot_chooser.initialize(this->ot_conn_reader, this->ot_conn_writer);
        //
        //     std::uint64_t num_bits = num_bytes << 3;
        //     static_assert((halfgates_max_batch_size & 0x7) == 0);
        //     assert((num_bits & 0x7) == 0);
        //     for (std::uint64_t i = 0; i < num_bits; i += halfgates_max_batch_size) {
        //         std::uint64_t batch_size = std::min(halfgates_max_batch_size, num_bits - i);
        //         std::uint64_t num_blocks = (batch_size + crypto::block_num_bits - 1) / crypto::block_num_bits;
        //         assert((batch_size & 0x7) == 0);
        //
        //         crypto::block choices[num_blocks];
        //         choices[num_blocks - 1] = crypto::zero_block();
        //         this->input_reader.read_bytes(reinterpret_cast<std::uint8_t*>(&choices[0]), batch_size >> 3);
        //
        //         std::array<crypto::block, halfgates_max_batch_size>* batch = this->evaluator_input_labels.start_write_single_in_place();
        //         ot_chooser.choose(this->ot_conn_reader, this->ot_conn_writer, choices, batch->data(), batch_size);
        //         this->evaluator_input_labels.finish_write_single_in_place();
        //     }
        // });
        }
    }

    void run_halfgates(const EngineOptions& args) {
        std::string file_base = args.problem_name + "_" + std::to_string(args.self_id);
        std::string prog_file = file_base + ".memprog";
        std::string output_file = file_base + ".output";

        std::chrono::time_point<std::chrono::steady_clock> start;
        std::chrono::time_point<std::chrono::steady_clock> end;

        /* Validate the config.yaml file for running the computation. */

        util::Configuration& c = *args.config;
        if (c["parties"].get(garbler_party_id) == nullptr) {
            std::cerr << "Garbler not present in configuration file" << std::endl;
            std::abort();
        }
        if (c["parties"].get(evaluator_party_id) == nullptr) {
            std::cerr << "Evaluator not present in configuration file" << std::endl;
            std::abort();
        }
        if (c["parties"][garbler_party_id]["workers"].get_size() != c["parties"][evaluator_party_id]["workers"].get_size()) {
            std::cerr << "Garbler has " << c["parties"][garbler_party_id]["workers"].get_size() << " workers but evaluator has " << c["parties"][evaluator_party_id]["workers"].get_size() << " workers --- must be equal" << std::endl;
            std::abort();
        }
        if (args.self_id >= c["parties"][garbler_party_id]["workers"].get_size()) {
            std::cerr << "Worker index is " << args.self_id << " but only " << c["parties"][garbler_party_id]["workers"].get_size() << " workers are specified" << std::endl;
            std::abort();
        }

        const util::ConfigValue& worker = c["parties"][evaluator_party_id]["workers"][args.self_id];

        OTInfo oti;
        if (worker.get("oblivious_transfer") != nullptr) {
            if (worker["oblivious_transfer"].get("max_batch_size") != nullptr) {
                std::int64_t temp = worker["oblivious_transfer"]["max_batch_size"].as_int();
                if (temp <= 0 || temp > SIZE_MAX || (temp & 7) != 0) {
                    std::cerr << "Specified \"oblivious_transfer/max_batch_size\" is " << temp << ", which is invalid" << std::endl;
                    std::abort();
                }
                oti.max_batch_size = static_cast<std::size_t>(temp);
            }
            if (worker["oblivious_transfer"].get("pipeline_depth") != nullptr) {
                std::int64_t temp = worker["oblivious_transfer"]["pipeline_depth"].as_int();
                if (temp <= 0 || temp > SIZE_MAX) {
                    std::cerr << "Specified \"oblivious_transfer/pipeline_depth\" is " << temp << ", which is invalid" << std::endl;
                    std::abort();
                }
                oti.pipeline_depth = static_cast<std::size_t>(temp);
            }
            if (worker["oblivious_transfer"].get("num_daemons") != nullptr) {
                std::int64_t temp = worker["oblivious_transfer"]["num_daemons"].as_int();
                if (temp <= 0 || temp > SIZE_MAX) {
                    std::cerr << "Specified \"oblivious_transfer/num_daemons\" is " << temp << ", which is invalid" << std::endl;
                    std::abort();
                }
                oti.num_daemons = static_cast<std::size_t>(temp);
            }
        }

        if (args.party_id == evaluator_party_id) {
            if (worker.get("external_host") == nullptr || worker.get("external_port") == nullptr) {
                std::cerr << "This party's external network information is not specified" << std::endl;
                std::abort();
            }

            std::string evaluator_input_file = file_base + "_evaluator.input";
            HalfGatesEvaluationEngine p(evaluator_input_file.c_str(), worker["external_port"].as_string().c_str(), oti);
            engine::ANDXOREngine executor(args.cluster, c["parties"][evaluator_party_id]["workers"][args.self_id], p, prog_file.c_str());
            start = std::chrono::steady_clock::now();
            executor.execute_program();
        } else if (args.party_id == garbler_party_id) {
            const util::ConfigValue& opposite_worker = c["parties"][evaluator_party_id]["workers"][args.self_id];
            if (opposite_worker.get("external_host") == nullptr || opposite_worker.get("external_port") == nullptr) {
                std::cerr << "Opposite party's external network information is not specified" << std::endl;
                std::abort();
            }

            std::string garbler_input_file = file_base + "_garbler.input";
            HalfGatesGarblingEngine p(args.cluster, garbler_input_file.c_str(), output_file.c_str(), opposite_worker["external_host"].as_string().c_str(), opposite_worker["external_port"].as_string().c_str(), oti);
            engine::ANDXOREngine executor(args.cluster, c["parties"][garbler_party_id]["workers"][args.self_id], p, prog_file.c_str());
            start = std::chrono::steady_clock::now();
            executor.execute_program();
        } else {
            std::cerr << "Party ID must be 0 or 1 (got " << args.party_id << ")" << std::endl;
            std::abort();
        }
        end = std::chrono::steady_clock::now();

        std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << ms.count() << " ms" << std::endl;
    }

    RegisterProtocol halfgates("halfgates", "Garbled Circuits with HalfGates optimizations", run_halfgates, "identity_plugin");
}
