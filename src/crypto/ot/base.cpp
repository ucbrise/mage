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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
#include "crypto/block.hpp"
#include "crypto/group.hpp"
#include "crypto/hash.hpp"
#include "util/filebuffer.hpp"

namespace mage::crypto::ot {
    /*
     * This implements the protocol given in Sections 2.3 and 3 of the
     * following paper:
     * M. Naor and B. Pinkas. Efficient Oblivious Transfer Protocols. SODA 2001.
     * Here is a link, for convenience: http://www.pinkas.net/PAPERS/effot.ps.
     *
     * I generalized it to support n rounds instead of 1.
     */

    static inline void send_group_element(util::BufferedFileWriter<false>& network_out, const DDHGroupElement& elem) {
        std::size_t size = elem.marshalled_uncompressed_size();
        network_out.write<std::size_t>() = size;
        void* into = network_out.start_write(size);
        elem.marshal_uncompressed(static_cast<std::uint8_t*>(into), size);
        network_out.finish_write(size);
    }

    static inline void receive_group_element(util::BufferedFileReader<false>& network_in, DDHGroupElement& elem) {
        std::size_t size = network_in.read<std::size_t>();
        const void* from = network_in.start_read(size);
        elem.unmarshal_uncompressed(static_cast<const std::uint8_t*>(from), size);
        network_in.finish_read(size);
    }

    static inline block hash_group_element_to_block(const DDHGroupElement& elem) {
        std::size_t size = elem.marshalled_uncompressed_size();
        std::uint8_t buffer[size] __attribute__((aligned(16)));
        elem.marshal_uncompressed(buffer, size);
        return hash_to_block(buffer, size);
    }

    struct BaseOTSenderDecision {
        BaseOTSenderDecision(const DDHGroup& g) : r(g), gr(g), pk0(g) {
        }
        ScalarMod r;
        DDHGroupElement gr;
        DDHGroupElement pk0;
    };

    void base_sender(const DDHGroup& g, util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out, const std::vector<std::pair<block, block>>& choices) {
        DDHGroupElement c(g);
        c.set_generator();
        send_group_element(network_out, c);

        network_out.flush();

        std::size_t num_choices = choices.size();
        std::vector<BaseOTSenderDecision> decisions;
        decisions.reserve(num_choices);
        for (std::size_t i = 0; i != num_choices; i++) {
            decisions.emplace_back(g);
            BaseOTSenderDecision& decision = decisions[i];
            receive_group_element(network_in, decision.pk0);
        }

        DDHGroupElement pk1(g);
        for (std::size_t i = 0; i != num_choices; i++) {
            BaseOTSenderDecision& decision = decisions[i];
            decision.r.set_random();
            decision.gr.multiply_generator(decision.r);
            send_group_element(network_out, decision.gr);
            decision.gr.multiply_restrict(decision.pk0, decision.r);
            decision.pk0.invert();
            pk1.add(c, decision.pk0);

            block ciphertext0 = xorBlocks(hash_group_element_to_block(decision.gr), choices[i].first);
            block_store_unaligned(ciphertext0, &network_out.write<block>());
            decision.gr.multiply_restrict(pk1, decision.r);
            block ciphertext1 = xorBlocks(hash_group_element_to_block(decision.gr), choices[i].second);
            block_store_unaligned(ciphertext1, &network_out.write<block>());
        }

        network_out.flush();
    }

    void base_chooser(const DDHGroup& g, util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out, std::vector<bool> choices, block* results) {
        std::size_t num_choices = choices.size();

        DDHGroupElement c(g);
        receive_group_element(network_in, c);

        std::vector<ScalarMod> k;
        k.reserve(num_choices);
        DDHGroupElement key0(g);
        for (std::size_t i = 0; i != num_choices; i++) {
            k.emplace_back(g);
            k[i].set_random();
            key0.multiply_generator(k[i]);
            if (choices[i]) {
                key0.invert();
                key0.add(key0, c);
            }
            send_group_element(network_out, key0);
        }
        network_out.flush();

        DDHGroupElement gr(g);
        for (std::size_t i = 0; i != num_choices; i++) {
            receive_group_element(network_in, gr);
            key0.multiply_restrict(gr, k[i]);
            void* data = network_in.start_read(2 * sizeof(block));
            block* ciphertexts = static_cast<block*>(data);
            block ciphertext = block_load_unaligned(choices[i] ? &ciphertexts[1] : &ciphertexts[0]);
            results[i] = xorBlocks(ciphertext, hash_group_element_to_block(key0));
            network_in.finish_read(2 * sizeof(block));
        }
    }
}
