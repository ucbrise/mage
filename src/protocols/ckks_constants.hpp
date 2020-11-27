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

#ifndef MAGE_SCHEMES_CKKS_CONSTANTS_HPP_
#define MAGE_SCHEMES_CKKS_CONSTANTS_HPP_

#include <cstdint>
#include <cstdlib>

namespace mage::protocols::ckks {
    inline double ckks_scale = std::pow(2.0, 40);

    constexpr std::uint64_t ckks_ciphertext_size(std::int32_t level, bool normalized) {
        if (normalized) {
            if (level == 0) {
                return 131689;
            } else if (level == 1) {
                return 263273;
            } else if (level == 2) {
                return 394857;
            } else {
                return UINT64_MAX;
            }
        } else {
            if (level == 0) {
                return UINT64_MAX;
            } else if (level == 1) {
                return 394857;
            } else if (level == 2) {
                return 592233;
            } else {
                return UINT64_MAX;
            }
        }
    }

    constexpr std::uint64_t ckks_plaintext_size(std::int32_t level) {
        /* For now, I've just taken the ciphertext sizes... */
        if (level == 0) {
            return 131689;
        } else if (level == 1) {
            return 263273;
        } else if (level == 2) {
            return 394857;
        } else {
            return UINT64_MAX;
        }
    }
}

#endif
