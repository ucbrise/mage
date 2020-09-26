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

#ifndef MAGE_UTIL_MISC_HPP_
#define MAGE_UTIL_MISC_HPP_

#include <cstdint>
#include <utility>

namespace mage::util {
    std::pair<std::int64_t, std::int64_t> floor_div(std::int64_t dividend, std::int64_t divisor) {
        int64_t quotient = dividend / divisor;
        int64_t remainder = dividend % divisor;
        if (remainder < 0) {
            quotient -= 1;
            remainder += divisor;
        }
        return std::make_pair(quotient, remainder);
    }

    std::pair<std::int64_t, std::int64_t> ceil_div(std::int64_t dividend, std::int64_t divisor) {
        int64_t quotient = dividend / divisor;
        int64_t remainder = dividend % divisor;
        if (remainder > 0) {
            quotient += 1;
            remainder -= divisor;
        }
        return std::make_pair(quotient, remainder);
    }
}

#endif
