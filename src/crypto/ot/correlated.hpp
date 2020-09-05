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

#ifndef MAGE_CRYPTO_OT_CORRELATED_HPP_
#define MAGE_CRYPTO_OT_CORRELATED_HPP_

#include <cstddef>
#include "crypto/block.hpp"
#include "crypto/ot/extension.hpp"
#include "util/filebuffer.hpp"

namespace mage::crypto::ot {
    class CorrelatedExtensionSender : public ExtensionSender {
    public:
        void send(util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out, block delta, block* first_choices, std::size_t num_choices);

    protected:
        void finish_send(block delta, block* first_choices, std::size_t num_choices, block* y, const block* qT);
    };

    class CorrelatedExtensionChooser : public ExtensionChooser {
    public:
        void choose(util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out, const block* choices, block* results, std::size_t num_choices);

    protected:
        void finish_choose(const block* choices, block* results, std::size_t num_choices, const block* y, const block* tT);
    };
}

#endif
