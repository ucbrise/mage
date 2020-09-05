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

#define BOOST_TEST_DYN_LINK
#include "boost/container/vector.hpp"
#include "boost/test/unit_test.hpp"
#include "boost/test/data/test_case.hpp"
#include "boost/test/data/monomorphic.hpp"

#include <cstdlib>
#include <algorithm>
#include <functional>
#include <thread>
#include <utility>
#include "crypto/block.hpp"
#include "crypto/ot/base.hpp"
#include "crypto/ot/extension.hpp"
#include "crypto/ot/correlated.hpp"
#include "platform/network.hpp"

namespace bdata = boost::unit_test::data;
using namespace mage;

struct TestNetwork {
    TestNetwork() {
        int pipe_descriptors[2];
        platform::pipe_open(pipe_descriptors);
        chooser_in.set_file_descriptor(pipe_descriptors[0], true);
        sender_out.set_file_descriptor(pipe_descriptors[1], true);
        platform::pipe_open(pipe_descriptors);
        sender_in.set_file_descriptor(pipe_descriptors[0], true);
        chooser_out.set_file_descriptor(pipe_descriptors[1], true);
    }
    util::BufferedFileReader<false> chooser_in;
    util::BufferedFileWriter<false> chooser_out;
    util::BufferedFileReader<false> sender_in;
    util::BufferedFileWriter<false> sender_out;
};

void test_ot(std::uint64_t batch_size, std::function<void(boost::container::vector<bool>&, std::vector<crypto::block>&)> run_ot) {
    // std::vector<bool> is optimized to a bit vector, so unfortunately, I have to do this manually
    boost::container::vector<bool> bits(batch_size);
    for (int i = 0; i != batch_size; i++) {
        bits[i] = ((rand() & 0x1) != 0x0);
    }
    std::uint64_t batch_size_blocks = (batch_size + crypto::block_num_bits - 1) / crypto::block_num_bits;
    std::vector<crypto::block> dense_bits(batch_size_blocks);
    for (std::uint64_t i = 0, k = 0; i < batch_size; (i += crypto::block_num_bits), k++) {
        std::uint64_t until = std::min((std::uint64_t) crypto::block_num_bits, batch_size - i);
        unsigned __int128& x = *reinterpret_cast<unsigned __int128*>(&dense_bits[k]);
        x = 0;
        for (std::uint64_t j = 0; j != until; j++) {
            if (bits[i + j]) {
                x |= (((unsigned __int128) 1) << j);
            }
        }
    }

    run_ot(bits, dense_bits);
}

BOOST_DATA_TEST_CASE(test_ot_base_single, bdata::xrange(1, 257), batch_size) {
    TestNetwork net;
    test_ot(batch_size, [&](boost::container::vector<bool>& bits, std::vector<crypto::block>& dense_bits) {
        std::vector<std::pair<crypto::block, crypto::block>> choices(batch_size);
        for (std::uint64_t i = 0; i != batch_size; i++) {
            std::pair<crypto::block, crypto::block>& p = choices[i];
            *reinterpret_cast<__int128*>(&p.first) = rand();
            *reinterpret_cast<__int128*>(&p.second) = rand();
        }

        std::vector<crypto::block> results(batch_size);

        std::thread t1([&]() {
            crypto::DDHGroup g;
            crypto::ot::base_send(g, net.sender_in, net.sender_out, choices.data(), batch_size);
        });

        std::thread t2([&]() {
            crypto::DDHGroup g;
            crypto::ot::base_choose(g, net.chooser_in, net.chooser_out, bits.data(), results.data(), batch_size);
        });

        t1.join();
        t2.join();

        for (std::uint64_t i = 0; i != batch_size; i++) {
            __int128& first = *reinterpret_cast<__int128*>(&choices[i].first);
            __int128& second = *reinterpret_cast<__int128*>(&choices[i].second);
            __int128& result = *reinterpret_cast<__int128*>(&results[i]);
            __int128& expected = bits[i] ? second : first;
            BOOST_CHECK_MESSAGE(result == expected, "For i = " << i << ", choices were (" << ((int) first) << ", " << ((int) second) << ") and bit was " << bits[i] << " but got " << ((int) result));
        }
    });
}

BOOST_DATA_TEST_CASE(test_ot_extension_single, bdata::xrange(1, 257) + bdata::xrange(383, 1407, 128) + bdata::xrange(384, 1408, 128) + bdata::xrange(385, 1409, 128), batch_size) {
    TestNetwork net;
    test_ot(batch_size, [&](boost::container::vector<bool>& bits, std::vector<crypto::block>& dense_bits) {
        std::vector<std::pair<crypto::block, crypto::block>> choices(batch_size);
        for (std::uint64_t i = 0; i != batch_size; i++) {
            std::pair<crypto::block, crypto::block>& p = choices[i];
            *reinterpret_cast<__int128*>(&p.first) = rand();
            *reinterpret_cast<__int128*>(&p.second) = rand();
        }

        std::vector<crypto::block> results(batch_size);

        std::thread t1([&]() {
            crypto::ot::ExtensionSender ot_sender;
            ot_sender.initialize(net.sender_in, net.sender_out);
            ot_sender.send(net.sender_in, net.sender_out, choices.data(), batch_size);
        });

        std::thread t2([&]() {
            crypto::ot::ExtensionChooser ot_chooser;
            ot_chooser.initialize(net.chooser_in, net.chooser_out);
            ot_chooser.choose(net.chooser_in, net.chooser_out, dense_bits.data(), results.data(), batch_size);
        });

        t1.join();
        t2.join();

        for (std::uint64_t i = 0; i != batch_size; i++) {
            __int128& first = *reinterpret_cast<__int128*>(&choices[i].first);
            __int128& second = *reinterpret_cast<__int128*>(&choices[i].second);
            __int128& result = *reinterpret_cast<__int128*>(&results[i]);
            __int128& expected = bits[i] ? second : first;
            BOOST_CHECK_MESSAGE(result == expected, "For i = " << i << ", choices were (" << ((int) first) << ", " << ((int) second) << ") and bit was " << bits[i] << " but got " << ((int) result));
        }
    });
}

BOOST_DATA_TEST_CASE(test_ot_correlated_extension_single, bdata::xrange(1, 257) + bdata::xrange(383, 1407, 128) + bdata::xrange(384, 1408, 128) + bdata::xrange(385, 1409, 128), batch_size) {
    TestNetwork net;
    test_ot(batch_size, [&](boost::container::vector<bool>& bits, std::vector<crypto::block>& dense_bits) {
        std::vector<crypto::block> first_choices(batch_size);
        std::vector<crypto::block> results(batch_size);
        crypto::block delta = crypto::makeBlock(rand(), rand());

        std::thread t1([&]() {
            crypto::ot::CorrelatedExtensionSender ot_sender;
            ot_sender.initialize(net.sender_in, net.sender_out);
            ot_sender.send(net.sender_in, net.sender_out, delta, first_choices.data(), batch_size);
        });

        std::thread t2([&]() {
            crypto::ot::CorrelatedExtensionChooser ot_chooser;
            ot_chooser.initialize(net.chooser_in, net.chooser_out);
            ot_chooser.choose(net.chooser_in, net.chooser_out, dense_bits.data(), results.data(), batch_size);
        });

        t1.join();
        t2.join();

        for (std::uint64_t i = 0; i != batch_size; i++) {
            __int128 first = *reinterpret_cast<__int128*>(&first_choices[i]);
            __int128 second = first ^ *reinterpret_cast<__int128*>(&delta);
            __int128& result = *reinterpret_cast<__int128*>(&results[i]);
            __int128 expected = bits[i] ? second : first;
            BOOST_CHECK_MESSAGE(result == expected, "For i = " << i << ", choices were (" << ((int) first) << ", " << ((int) second) << ") and bit was " << bits[i] << " but got " << ((int) result));
        }
    });
}
