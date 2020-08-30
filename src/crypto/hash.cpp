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

#include "crypto/hash.hpp"
#include <cstddef>
#include <cstdint>
#include <openssl/sha.h>
#include "crypto/block.hpp"

namespace mage::crypto {
    void hash(const void* src, std::size_t src_length, std::uint8_t* into) {
        SHA256(static_cast<const unsigned char*>(src), src_length, into);
    }

    block hash_to_block(const void* src, std::size_t src_length) {
        std::uint8_t into[hash_length] __attribute__((aligned(sizeof(block))));
        hash(src, src_length, into);
        return *reinterpret_cast<block*>(&into[0]);
    }
}
