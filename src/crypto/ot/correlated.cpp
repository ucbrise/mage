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

#include "crypto/ot/correlated.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include "crypto/block.hpp"
#include "crypto/ot/extension.hpp"
#include "util/filebuffer.hpp"
#include "util/userpipe.hpp"

namespace mage::crypto::ot {
    void CorrelatedExtensionSender::finish_send(block delta, block* first_choices, std::size_t num_choices, block* y, const block* qT) {
        assert(this->initialized);

        for (std::uint32_t j = 0; j != num_choices; j++) { // iterating over rows (columns of transpose)
            const block* qj = &qT[j];
            {
                Hasher h1(&j, sizeof(j)); // TODO: marshal this
                h1.update(qj, sizeof(block));
                first_choices[j] = h1.output_block();
            }
            Hasher h2(&j, sizeof(j)); // TODO: marshal this
            block temp = xorBlocks(block_load_unaligned(qj), this->s);
            h2.update(&temp, sizeof(block));
            y[j] = xorBlocks(delta, xorBlocks(first_choices[j], h2.output_block()));
        }
    }

    void CorrelatedExtensionSender::send(util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out, block delta, block* first_choices, std::size_t num_choices) {
        assert(num_choices != 0); // see_trans does not work for zero-size matrices

        std::size_t num_row_blocks = (num_choices + block_num_bits - 1) / block_num_bits;
        std::size_t num_blocks = num_row_blocks * extension_kappa;

        block q[num_blocks];
        block qT[num_blocks];

        void* from = network_in.start_read(sizeof(block) * num_blocks);
        block* u = reinterpret_cast<block*>(from);
        this->prepare_send(num_choices, u, q);
        network_in.finish_read(sizeof(block) * num_blocks);

        sse_trans(reinterpret_cast<std::uint8_t*>(qT), reinterpret_cast<std::uint8_t*>(q), extension_kappa, num_row_blocks * block_num_bits);

        network_out.flush(); // guarantees that y will be aligned
        void* into = network_out.start_write(sizeof(block) * num_choices);
        block* y = static_cast<block*>(into);
        this->finish_send(delta, first_choices, num_choices, y, qT);
        network_out.finish_write(sizeof(block) * num_choices);
        network_out.flush();
    }

    void CorrelatedExtensionChooser::finish_choose(const block* choices, block* results, std::size_t num_choices, const block* y, const block* tT) {
        assert(this->initialized);

        for (std::uint32_t j = 0; j != num_choices; j++) {
            Hasher h(&j, sizeof(j)); // TODO: marshal this
            h.update(&tT[j], sizeof(block));
            if (block_bit(choices[j / block_num_bits], j % block_num_bits)) {
                results[j] = xorBlocks(y[j], h.output_block());
            } else {
                results[j] = h.output_block();
            }
        }
    }

    void CorrelatedExtensionChooser::choose(util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out, const block* choices, block* results, std::size_t num_choices) {
        assert(num_choices != 0); // sse_trans does not work for zero-size matrices

        std::size_t num_row_blocks = (num_choices + block_num_bits - 1) / block_num_bits;
        std::size_t num_blocks = num_row_blocks * extension_kappa;

        block t[num_blocks];
        block tT[num_blocks];

        network_out.flush(); // This guarantees that u will be aligned
        void* into = network_out.start_write(sizeof(block) * num_blocks);
        block* u = static_cast<block*>(into);
        this->prepare_choose(choices, num_choices, u, t);
        network_out.finish_write(sizeof(block) * num_blocks);
        network_out.flush();

        sse_trans(reinterpret_cast<std::uint8_t*>(tT), reinterpret_cast<std::uint8_t*>(t), extension_kappa, num_row_blocks * block_num_bits);

        void* from = network_in.start_read(sizeof(block) * num_choices);
        block* y = static_cast<block*>(from);
        this->finish_choose(choices, results, num_choices, y, tT);
        network_in.finish_read(sizeof(block) * num_choices);
    }

    PipelinedCorrelatedExtSender::PipelinedCorrelatedExtSender(util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out, block choice_delta, util::UserPipe<block>& results, std::size_t batch_size, std::size_t max_depth)
        : net_in(network_in), net_out(network_out), delta(choice_delta), output(results), num_choices(batch_size), depth(max_depth) {
        assert(this->num_choices != 0); // sse_trans does not work for zero-size matrices

        this->initialize(this->net_in, this->net_out);

        this->num_row_blocks = (this->num_choices + block_num_bits - 1) / block_num_bits;
        this->num_blocks = this->num_row_blocks * extension_kappa;
        this->pipeline = std::make_unique<util::UserPipe<crypto::block>>(this->num_blocks * this->depth);

        this->start_daemon();
    }

    PipelinedCorrelatedExtSender::~PipelinedCorrelatedExtSender() {
        this->pipeline->close();
        this->daemon.join();
    }

    void PipelinedCorrelatedExtSender::start_daemon() {
        this->daemon = std::thread([this]() {
            block* request;
            while ((request = this->pipeline->start_read_in_place(this->num_blocks)) != nullptr) {
                block* qT = request;
                this->net_out.flush();
                void* into = this->net_out.start_write(sizeof(block) * this->num_choices);
                block* y = static_cast<block*>(into);
                block* results = this->output.start_write_in_place(this->num_choices);
                this->finish_send(this->delta, results, this->num_choices, y, qT);
                this->output.finish_write_in_place(this->num_choices);
                this->pipeline->finish_read_in_place(this->num_blocks);
                this->net_out.finish_write(sizeof(block) * this->num_choices);
                this->net_out.flush();
            }
        });
    }

    /* WARNING: Do not call this concurrently from multiple threads. */
    void PipelinedCorrelatedExtSender::submit_send() {
        block q[this->num_blocks];
        block* request = this->pipeline->start_write_in_place(this->num_blocks);
        assert(request != nullptr);
        block* qT = request;

        void* from = this->net_in.start_read(sizeof(block) * this->num_blocks);
        block* u = reinterpret_cast<block*>(from);
        this->prepare_send(this->num_choices, u, q);
        this->net_in.finish_read(sizeof(block) * this->num_blocks);

        sse_trans(reinterpret_cast<std::uint8_t*>(qT), reinterpret_cast<std::uint8_t*>(q), extension_kappa, this->num_row_blocks * block_num_bits);

        this->pipeline->finish_write_in_place(this->num_blocks);
    }

    PipelinedCorrelatedExtChooser::PipelinedCorrelatedExtChooser(util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out, util::UserPipe<block>& results, std::size_t batch_size, std::size_t max_depth)
        : net_in(network_in), net_out(network_out), output(results), num_choices(batch_size), depth(max_depth) {
        assert(this->num_choices != 0); // sse_trans does not work for zero-size matrices

        this->initialize(this->net_in, this->net_out);

        this->num_row_blocks = (this->num_choices + block_num_bits - 1) / block_num_bits;
        this->num_blocks = this->num_row_blocks * extension_kappa;
        this->pipeline = std::make_unique<util::UserPipe<crypto::block>>((this->num_row_blocks + this->num_blocks) * this->depth);

        this->start_daemon();
    }

    PipelinedCorrelatedExtChooser::~PipelinedCorrelatedExtChooser() {
        this->pipeline->close();
        this->daemon.join();
    }

    void PipelinedCorrelatedExtChooser::start_daemon() {
        this->daemon = std::thread([this]() {
            block* request;
            while ((request = this->pipeline->start_read_in_place(this->num_row_blocks + this->num_blocks)) != nullptr) {
                block* choices = request;
                block* tT = request + this->num_row_blocks;
                void* from = this->net_in.start_read(sizeof(block) * this->num_choices);
                block* y = static_cast<block*>(from);
                block* results = this->output.start_write_in_place(this->num_blocks);
                this->finish_choose(choices, results, this->num_choices, y, tT);
                this->output.finish_write_in_place(this->num_blocks);
                this->pipeline->finish_read_in_place(this->num_row_blocks + this->num_blocks);
                this->net_in.finish_read(sizeof(block) * this->num_choices);
            }
        });
    }

    /* WARNING: Do not call this concurrently from multiple threads. */
    void PipelinedCorrelatedExtChooser::submit_choose(const block* choices) {
        block t[this->num_blocks];
        block* request = this->pipeline->start_write_in_place(this->num_row_blocks + this->num_blocks);
        assert(request != nullptr);
        std::copy(choices, choices + this->num_row_blocks, request);
        block* tT = request + this->num_row_blocks;

        this->net_out.flush(); // This guarantees that u will be aligned
        void* into = this->net_out.start_write(sizeof(block) * this->num_blocks);
        block* u = static_cast<block*>(into);
        this->prepare_choose(choices, this->num_choices, u, t);
        this->net_out.finish_write(sizeof(block) * this->num_blocks);
        this->net_out.flush();

        sse_trans(reinterpret_cast<std::uint8_t*>(tT), reinterpret_cast<std::uint8_t*>(t), extension_kappa, this->num_row_blocks * block_num_bits);

        this->pipeline->finish_write_in_place(this->num_row_blocks + this->num_blocks);
    }
}
