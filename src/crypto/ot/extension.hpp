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

#ifndef MAGE_CRYPTO_OT_EXTENSION_HPP_
#define MAGE_CRYPTO_OT_EXTENSION_HPP_

#include <cstddef>
#include <cstdint>
#include <array>
#include <utility>
#include "crypto/block.hpp"
#include "crypto/ot/base.hpp"
#include "util/filebuffer.hpp"

namespace mage::crypto::ot {
    /*
     * This implements OT Extension based on the protocol in Section 5.3 and
     * Protocol 5.2 of the following paper:
     * G. Asharov, Y. Lindell, T. Schneider, and M. Zohner. More Efficient
     * Oblivious Transfer and Extensions for Faster Secure Computation. CCS
     * 2013.
     */

    /* Kappa is the symmetric security parameter. */
    constexpr const std::uint8_t extension_kappa = block_num_bits;

    enum class ExtensionState : std::uint8_t {
        Uninitialized,
        Ready,
        Prepared
    };

    class ExtensionSender {
    public:
        ExtensionSender();
        void initialize(util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out);

        void send(util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out, const std::pair<block, block>* choices, std::size_t num_choices);

    private:
        void prepare_send(util::BufferedFileReader<false>& network_in, std::size_t num_choices, block* q);
        void finish_send(util::BufferedFileWriter<false>& network_out, const std::pair<block, block>* choices, std::size_t num_choices, const block* qT);

        std::array<PRG, extension_kappa> prgs;
        block s;

        ExtensionState state;
    };

    struct ExtChooserPRGs {
        PRG g0;
        PRG g1;
    };

    class ExtensionChooser {
    public:
        ExtensionChooser();
        void initialize(util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out);

        void choose(util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out, const block* choices, block* results, std::size_t num_choices);

    private:
        void prepare_choose(util::BufferedFileWriter<false>& network_out, const block* choices, std::size_t num_choices, block* t);
        void finish_choose(util::BufferedFileReader<false>& network_in, const block* choices, block* results, std::size_t num_choices, const block* tT);

        std::array<ExtChooserPRGs, extension_kappa> prgs;

        ExtensionState state;
    };
}

#endif
