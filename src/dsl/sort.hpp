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
#include "dsl/array.hpp"
#include "util/misc.hpp"

namespace mage::dsl {
    /*
     * BITONIC-SORTER[length] network (see Algorithms by CLR, Section 28.3).
     * Sorts a bitonic sequence.
     */
    template <typename T>
    void bitonic_sorter(T* array, std::uint64_t length, std::uint64_t max_depth = UINT64_MAX) {
        assert(util::is_power_of_two(length));

        /* Base case */
        if (length == 1 || max_depth == 0) {
            return;
        }

        std::uint64_t half_length = length >> 1;

        /* HALF-CLEANER[length] network (see CLR, Section 28.3). */
        for (std::uint64_t i = 0; i < half_length; i++) {
            T::comparator(array[i], array[i + half_length]);
        }

        bitonic_sorter<T>(array, half_length, max_depth - 1);
        bitonic_sorter<T>(&array[half_length], half_length, max_depth - 1);
    }

    template <typename T>
    void parallel_bitonic_sorter(ShardedArray<T>& array) {
        std::vector<T>& locals = array.get_locals();
        std::uint64_t length = locals.size();

        assert(util::is_power_of_two(length));
        assert(util::is_power_of_two(array.get_num_proc()));
        assert(array.get_layout() == Layout::Cyclic);

        std::uint64_t total_phases = util::log_base_2(length * array.get_num_proc());
        std::uint64_t first_pass_phases = total_phases - util::log_base_2(length);
        bitonic_sorter<T>(locals.data(), length, first_pass_phases);
        array.switch_layout(Layout::Blocked);
        bitonic_sorter<T>(locals.data(), length);
    }
}

#endif
