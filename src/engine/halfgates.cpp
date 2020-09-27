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
#include "crypto/ot/correlated.hpp"
#include "util/filebuffer.hpp"

namespace mage::engine {
    void HalfGatesGarblingEngine::start_input_daemon() {
        this->input_daemon = std::thread([=]() {
            crypto::ot::CorrelatedExtensionSender ot_sender;
            std::uint64_t num_bytes = this->ot_conn_reader.read<std::uint64_t>();

            ot_sender.initialize(this->ot_conn_reader, this->ot_conn_writer);

            std::uint64_t num_bits = num_bytes << 3;
            static_assert((halfgates_max_batch_size & 0x7) == 0);
            assert((num_bits & 0x7) == 0);
            for (std::uint64_t i = 0; i < num_bits; i += halfgates_max_batch_size) {
                std::uint64_t batch_size = std::min(halfgates_max_batch_size, num_bits - i);
                // std::pair<crypto::block, crypto::block>* ot_pairs = new std::pair<crypto::block, crypto::block>[batch_size];
                // {
                //     Wire* batch = new Wire[batch_size];
                //     this->garbler.input_evaluator(batch, batch_size, ot_pairs);
                //     this->evaluator_input_labels.write_contiguous(batch, batch_size);
                //     delete[] batch;
                // }
                // ot_sender.send(this->ot_conn_reader, this->ot_conn_writer, ot_pairs, batch_size);
                // delete[] ot_pairs;

                std::array<crypto::block, halfgates_max_batch_size>& batch = *(this->evaluator_input_labels.start_write_single_in_place());
                ot_sender.send(this->ot_conn_reader, this->ot_conn_writer, this->garbler.get_delta(), batch.data(), batch_size);
                this->evaluator_input_labels.finish_write_single_in_place();
            }
        });
    }

    void HalfGatesEvaluationEngine::start_input_daemon() {
        this->input_daemon = std::thread([=]() {
            crypto::ot::CorrelatedExtensionChooser ot_chooser;
            std::uint64_t num_bytes = this->input_reader.get_file_length();
            this->ot_conn_writer.write<std::uint64_t>() = num_bytes;
            this->ot_conn_writer.flush();

            ot_chooser.initialize(this->ot_conn_reader, this->ot_conn_writer);

            std::uint64_t num_bits = num_bytes << 3;
            static_assert((halfgates_max_batch_size & 0x7) == 0);
            assert((num_bits & 0x7) == 0);
            for (std::uint64_t i = 0; i < num_bits; i += halfgates_max_batch_size) {
                std::uint64_t batch_size = std::min(halfgates_max_batch_size, num_bits - i);
                std::uint64_t num_blocks = (batch_size + crypto::block_num_bits - 1) / crypto::block_num_bits;
                assert((batch_size & 0x7) == 0);

                crypto::block choices[num_blocks];
                choices[num_blocks - 1] = crypto::zero_block();
                this->input_reader.read_bytes(reinterpret_cast<std::uint8_t*>(&choices[0]), batch_size >> 3);

                std::array<crypto::block, halfgates_max_batch_size>* batch = this->evaluator_input_labels.start_write_single_in_place();
                ot_chooser.choose(this->ot_conn_reader, this->ot_conn_writer, choices, batch->data(), batch_size);
                this->evaluator_input_labels.finish_write_single_in_place();
            }
        });
    }
}
