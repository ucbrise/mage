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

#ifndef MAGE_CRYPTO_HASH_HPP_
#define MAGE_CRYPTO_HASH_HPP_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <openssl/sha.h>
#include "crypto/block.hpp"

namespace mage::crypto {
    constexpr const std::uint16_t hash_length = SHA256_DIGEST_LENGTH;
    void hash(const void* src, std::size_t src_length, std::uint8_t* into);
    block hash_to_block(const void* src, std::size_t src_length);

    class Hasher {
    public:
        static constexpr const std::uint32_t output_length = SHA256_DIGEST_LENGTH;

        Hasher() : active(true) {
            SHA256_Init(&this->ctx);
        }

        Hasher(const void* src, std::size_t src_length) : Hasher() {
            this->update(src, src_length);
        }

        void update(const void* src, std::size_t src_length) {
            assert(this->active);
            SHA256_Update(&this->ctx, src, src_length);
        }

        void output(std::uint8_t* into) {
            assert(this->active);
            this->active = false;
            SHA256_Final(into, &this->ctx);
        }

        block output_block() {
            std::uint8_t into[hash_length] __attribute__((aligned(sizeof(block))));
            this->output(into);
            return *reinterpret_cast<block*>(&into[0]);
        }

    private:
        SHA256_CTX ctx;
        bool active;
    };
}

#endif
