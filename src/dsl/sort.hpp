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

#ifndef MAGE_DSL_SORT_HPP_
#define MAGE_DSL_SORT_HPP_

#include <cassert>
#include <cstdint>

namespace mage::dsl {
    /*
     * BITONIC-SORTER[length] network (see Algorithms by CLR, Section 28.3).
     * Sorts a bitonic sequence.
     */
    template <typename T>
    void bitonic_sorter(T* array, std::uint64_t length) {
        assert((length & (length - 1)) == 0); // length must be a power of two

        /* Base case */
        if (length == 1) {
            return;
        }

        std::uint64_t half_length = length >> 1;

        /* HALF-CLEANER[length] network (see CLR, Section 28.3). */
        for (std::uint64_t i = 0; i < half_length; i++) {
            T::comparator(array[i], array[i + half_length]);
        }

        bitonic_sorter<T>(array, half_length);
        bitonic_sorter<T>(&array[half_length], half_length);
    }
}

#endif
