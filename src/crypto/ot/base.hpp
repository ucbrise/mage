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
    /* Basic oblivious transfer, on top of which we can implement OT extension. */
    void base_sender(const DDHGroup& g, util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out, const std::vector<std::pair<block, block>>& choices);
    void base_chooser(const DDHGroup& g, util::BufferedFileReader<false>& network_in, util::BufferedFileWriter<false>& network_out, std::vector<bool> choices, block* results);
}
