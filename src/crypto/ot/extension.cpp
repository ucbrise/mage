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

#include "crypto/ot/extension.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <utility>
#include "crypto/group.hpp"
#include "crypto/block.hpp"
#include "crypto/hash.hpp"
#include "util/filebuffer.hpp"

namespace mage::crypto::ot {
    ExtensionSender::ExtensionSender() : state(ExtensionState::Uninitialized) {
    }

    void ExtensionSender::initialize(util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out) {
        assert(this->state == ExtensionState::Uninitialized);

        PRG prg;
        prg.random_block(&this->s);

        std::array<bool, extension_kappa> s_array;
        unsigned __int128 integer_s = *reinterpret_cast<unsigned __int128*>(&this->s);
        for (int i = 0; i != extension_kappa; i++) {
            s_array[i] = ((integer_s >> i) & 0x1) != 0x0;
        }

        DDHGroup g;
        std::array<block, extension_kappa> k;
        base_choose(g, network_in, network_out, s_array.data(), k.data(), extension_kappa);

        for (int i = 0; i != extension_kappa; i++) {
            this->prgs[i].set_seed(k[i]);
        }

        this->state = ExtensionState::Ready;
    }

    void ExtensionSender::prepare_send(util::BufferedFileReader<false>& network_in, std::size_t num_choices, block* q) {
        assert(this->state == ExtensionState::Ready);

        /* The variable m in the paper is num_choices. */
        std::size_t num_row_blocks = (num_choices + block_num_bits - 1) / block_num_bits;
        std::size_t num_blocks = num_row_blocks * extension_kappa;
        /* q and qT are uninitialied arrays of num_blocks blocks. */

        void* from = network_in.start_read(sizeof(block) * num_blocks);
        block* u = reinterpret_cast<block*>(from);
        unsigned __int128 integer_s = *reinterpret_cast<unsigned __int128*>(&this->s);
        for (std::size_t i = 0, k = 0; i != num_blocks; (i += num_row_blocks), k++) {
            this->prgs[k].random_block(&q[i], num_row_blocks);
            if (block_bit(this->s, i)) {
                for (std::size_t j = 0; j != num_row_blocks; j++) {
                    block x = block_load_unaligned(&u[i + j]);
                    q[i + j] = xorBlocks(x, q[i + j]);
                }
            }
        }
        network_in.finish_read(sizeof(block) * num_blocks);

        this->state = ExtensionState::Prepared;
    }

    void ExtensionSender::finish_send(util::BufferedFileWriter<false>& network_out, const std::pair<block, block>* choices, std::size_t num_choices, const block* qT) {
        assert(this->state == ExtensionState::Prepared);

        network_out.flush(); // guarantees that y will be aligned
        void* into = network_out.start_write(2 * sizeof(block) * num_choices);
        block* y = static_cast<block*>(into);
        for (std::uint32_t j = 0; j != num_choices; j++) { // iterating over rows (columns of transpose)
            const block* qj = &qT[j];
            {
                Hasher h(&j, sizeof(j)); // TODO: marshal this
                h.update(qj, sizeof(block));
                y[j << 1] = xorBlocks(choices[j].first, h.output_block());
            }
            {
                Hasher h(&j, sizeof(j)); // TODO: marshal this
                block temp = xorBlocks(block_load_unaligned(qj), this->s);
                h.update(&temp, sizeof(block));
                y[(j << 1) | 0x1] = xorBlocks(choices[j].second, h.output_block());
            }
        }
        network_out.finish_write(2 * sizeof(block) * num_choices);
        network_out.flush();

        this->state = ExtensionState::Ready;
    }

    void ExtensionSender::send(util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out, const std::pair<block, block>* choices, std::size_t num_choices) {
        std::size_t num_row_blocks = (num_choices + block_num_bits - 1) / block_num_bits;
        std::size_t num_blocks = num_row_blocks * extension_kappa;

        block buffer[2 * num_blocks];
        block* q = &buffer[0];
        block* qT = &buffer[num_blocks];

        this->prepare_send(network_in, num_choices, q);
        sse_trans(reinterpret_cast<std::uint8_t*>(qT), reinterpret_cast<std::uint8_t*>(q), num_row_blocks * block_num_bits, extension_kappa);
        this->finish_send(network_out, choices, num_choices, qT);
    }

    ExtensionChooser::ExtensionChooser() : state(ExtensionState::Uninitialized) {
    }

    void ExtensionChooser::initialize(util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out) {
        assert(this->state == ExtensionState::Uninitialized);

        PRG prg;
        std::array<std::pair<block, block>, extension_kappa> k;
        for (int i = 0; i != extension_kappa; i++) {
            prg.random_block(&k[i].first);
            prg.random_block(&k[i].second);
        }

        DDHGroup g;
        base_send(g, network_in, network_out, k.data(), extension_kappa);

        for (int i = 0; i != extension_kappa; i++) {
            this->prgs[i].g0.set_seed(k[i].first);
            this->prgs[i].g1.set_seed(k[i].second);
        }

        this->state = ExtensionState::Ready;
    }

    void ExtensionChooser::prepare_choose(util::BufferedFileWriter<false>& network_out, const block* choices, std::size_t num_choices, block* t) {
        assert(this->state == ExtensionState::Ready);

        /* The variable m in the paper is num_choices. */
        std::size_t num_row_blocks = (num_choices + block_num_bits - 1) / block_num_bits;
        std::size_t num_blocks = num_row_blocks * extension_kappa;
        /* q and qT are uninitialied arrays of num_blocks blocks. */

        network_out.flush(); // This guarantees that u will be aligned
        void* into = network_out.start_write(sizeof(block) * num_blocks);
        block* u = static_cast<block*>(into);
        for (std::size_t i = 0, k = 0; i != num_blocks; (i += num_row_blocks), k++) { // iterating over columns
            this->prgs[k].g0.random_block(&t[i], num_row_blocks);
            this->prgs[k].g1.random_block(&u[i], num_row_blocks);
            for (std::size_t j = 0; j != num_row_blocks; j++) { // iterating over segments of a single column
                u[i + j] = xorBlocks(u[i + j], t[i + j]);
                // unsigned __int128 integer_r = 0;
                // for (int a = 0; a != block_num_bits; a++) {
                //     if (choices[block_num_bits * j + a]) {
                //         unsigned __int128 one = 1;
                //         integer_r |= (one << a);
                //     }
                // }
                u[i + j] = xorBlocks(u[i + j], choices[j]);
            }
        }
        network_out.finish_write(sizeof(block) * num_blocks);
        network_out.flush();

        this->state = ExtensionState::Prepared;
    }

    void ExtensionChooser::finish_choose(util::BufferedFileReader<false>& network_in, const block* choices, block* results, std::size_t num_choices, const block* tT) {
        assert(this->state == ExtensionState::Prepared);

        void* from = network_in.start_read(2 * sizeof(block) * num_choices);
        block* y = static_cast<block*>(from);
        for (std::uint32_t j = 0; j != num_choices; j++) {
            Hasher h(&j, sizeof(j)); // TODO: marshal this
            block yj = block_bit(choices[j / block_num_bits], j % block_num_bits) ? y[(j << 1) + 1] : y[j << 1];
            h.update(&tT[j], sizeof(block));
            results[j] = xorBlocks(yj, h.output_block());
        }
        network_in.finish_read(2 * sizeof(block) * num_choices);

        this->state = ExtensionState::Ready;
    }

    void ExtensionChooser::choose(util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out, const block* choices, block* results, std::size_t num_choices) {
        std::size_t num_row_blocks = (num_choices + block_num_bits - 1) / block_num_bits;
        std::size_t num_blocks = num_row_blocks * extension_kappa;

        block buffer[2 * num_blocks];
        block* t = &buffer[0];
        block* tT = &buffer[num_blocks];

        this->prepare_choose(network_out, choices, num_choices, t);
        sse_trans(reinterpret_cast<std::uint8_t*>(tT), reinterpret_cast<std::uint8_t*>(t), num_row_blocks * block_num_bits, extension_kappa);
        this->finish_choose(network_in, choices, results, num_choices, tT);
    }
}
