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

#include "engine/halfgates.hpp"
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <thread>
#include "crypto/block.hpp"
#include "crypto/ot/extension.hpp"
#include "util/filebuffer.hpp"

namespace mage::engine {
    const constexpr std::uint64_t max_batch_size = 128;

    void HalfGatesGarblingEngine::start_input_daemon() {
        this->input_daemon = std::thread([=]() {
            std::uint64_t num_bytes = this->ot_conn_reader.read<std::uint64_t>();

            this->ot_sender.initialize(this->ot_conn_reader, this->ot_conn_writer);

            std::uint64_t num_bits = num_bytes << 3;
            static_assert((max_batch_size & 0x7) == 0);
            assert((num_bits & 0x7) == 0);
            for (std::uint64_t i = 0; i < num_bits; i += max_batch_size) {
                std::uint64_t batch_size = std::min(max_batch_size, num_bits - i);
                std::pair<crypto::block, crypto::block>* ot_pairs = new std::pair<crypto::block, crypto::block>[batch_size];
                {
                    Wire* batch = new Wire[batch_size];
                    this->garbler.input_evaluator(batch, batch_size, ot_pairs);
                    this->evaluator_input_labels.write_contiguous(batch, batch_size);
                    delete[] batch;
                }
                this->ot_sender.send(this->ot_conn_reader, this->ot_conn_writer, ot_pairs, batch_size);
                delete[] ot_pairs;
            }
        });
    }

    void HalfGatesEvaluationEngine::start_input_daemon() {
        this->input_daemon = std::thread([=]() {
            std::uint64_t num_bytes = this->input_reader.get_file_length();
            this->ot_conn_writer.write<std::uint64_t>() = num_bytes;
            this->ot_conn_writer.flush();

            this->ot_chooser.initialize(this->ot_conn_reader, this->ot_conn_writer);

            std::uint64_t num_bits = num_bytes << 3;
            static_assert((max_batch_size & 0x7) == 0);
            assert((num_bits & 0x7) == 0);
            for (std::uint64_t i = 0; i < num_bits; i += max_batch_size) {
                std::uint64_t batch_size = std::min(max_batch_size, num_bits - i);
                std::uint64_t num_blocks = (batch_size + crypto::block_num_bits - 1) / crypto::block_num_bits;
                assert((batch_size & 0x7) == 0);

                crypto::block* arr = new crypto::block[num_blocks + batch_size];

                crypto::block* choices = &arr[0];
                choices[num_blocks - 1] = crypto::zero_block();
                this->input_reader.read_bytes(reinterpret_cast<std::uint8_t*>(&choices[0]), batch_size >> 3);

                crypto::block* batch = &arr[num_blocks];
                this->ot_chooser.choose(this->ot_conn_reader, this->ot_conn_writer, choices, batch, batch_size);
                this->evaluator_input_labels.write_contiguous(batch, batch_size);

                delete[] arr;
            }
        });
    }
}
