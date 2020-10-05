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

#ifndef MAGE_UTIL_BINARY_HPP_
#define MAGE_UTIL_BINARY_HPP_

#include <cstdint>
#include <cstdlib>
#include <bit>
#include <istream>
#include <ostream>

namespace mage::util {
    inline void write_lower_bytes(std::ostream& out, std::uint8_t val, std::uint8_t num_bytes) {
        out.write(reinterpret_cast<const char*>(&val), num_bytes);
    }

    inline void read_lower_bytes(std::istream& in, std::uint8_t& val, std::uint8_t num_bytes) {
        in.read(reinterpret_cast<char*>(&val), num_bytes);
    }

    /* Based on https://stackoverflow.com/questions/45307516/c-c-code-to-convert-big-endian-to-little-endian */
    inline std::uint64_t swap64(std::uint64_t k) {
        return ((k << 56) |
                ((k & UINT64_C(0x000000000000FF00)) << 40) |
                ((k & UINT64_C(0x0000000000FF0000)) << 24) |
                ((k & UINT64_C(0x00000000FF000000)) << 8) |
                ((k & UINT64_C(0x000000FF00000000)) >> 8) |
                ((k & UINT64_C(0x0000FF0000000000)) >> 24) |
                ((k & UINT64_C(0x00FF000000000000)) >> 40) |
                (k >> 56));
      }

    inline void write_lower_bytes(std::ostream& out, std::uint64_t val, std::uint8_t num_bytes) {
        if constexpr(std::endian::native == std::endian::little) {
            out.write(reinterpret_cast<const char*>(&val), num_bytes);
        } else if constexpr(std::endian::native == std::endian::big) {
            val = swap64(val);
            out.write(reinterpret_cast<const char*>(&val), num_bytes);
        } else {
            /* Middle-endian architectures not supported. */
            std::abort();
        }
    }

    inline void read_lower_bytes(std::istream& in, std::uint64_t& val, std::uint8_t num_bytes) {
        val = 0;
        if constexpr(std::endian::native == std::endian::little) {
            in.read(reinterpret_cast<char*>(&val), num_bytes);
        } else if constexpr(std::endian::native == std::endian::big) {
            in.read(reinterpret_cast<char*>(&val), num_bytes);
            val = swap64(val);
        } else {
            /* Middle-endian architectures not supported. */
            std::abort();
        }
    }
}

#endif
