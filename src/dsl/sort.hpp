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
    void bitonic_sorter(T* array, std::uint64_t length, bool increasing = true, std::uint64_t max_depth = UINT64_MAX) {
        assert(util::is_power_of_two(length));

        /* Base case */
        if (length == 1 || length == 0 || max_depth == 0) {
            return;
        }

        std::uint64_t half_length = length >> 1;

        /* HALF-CLEANER[length] network (see CLR, Section 28.3). */
        for (std::uint64_t i = 0; i < half_length; i++) {
            if (increasing) {
                T::comparator(array[i], array[i + half_length]);
            } else {
                T::comparator(array[i + half_length], array[i]);
            }
        }

        bitonic_sorter<T>(array, half_length, increasing, max_depth - 1);
        bitonic_sorter<T>(array + half_length, half_length, increasing, max_depth - 1);
    }

    /*
     * Modified version of SORTER[length] network (see Algorithms by CLR,
     * Section 28.5). Unlike the MERGER circuit, which modifies the first
     * half-cleaner to effectively reverse the second half of the input, we
     * "flip" the recursive SORTER[length/2] circuit that produces the second
     * half of the MERGER's input to sort in the opposite direction. This
     * allows us to just use the BITONIC-SORTER circuit to merge the two sorted
     * arrays. This creates a more regular structure for the circuit. The "Fast
     * Parallel Sorting under LogP" paper uses this same trick, allowing for a
     * simpler communication schedule. That isn't important for this function,
     * but it is important for parallel_sorter, because we use the
     * communication schedule suggested in that paper.
     */
    template <typename T>
    void sorter(T* array, std::uint64_t length, bool increasing = true) {
        assert(util::is_power_of_two(length));

        if (length == 1 || length == 0) {
            return;
        }

        std::uint64_t half_length = length >> 1;
        sorter(array, half_length, true);
        sorter(array + half_length, half_length, false);
        bitonic_sorter(array, length, increasing);
    }

    template <typename T>
    void parallel_bitonic_sorter(ShardedArray<T>& array, bool increasing = true) {
        std::vector<T>& locals = array.get_locals();
        std::uint64_t length = locals.size();

        assert(util::is_power_of_two(length));
        assert(util::is_power_of_two(array.get_num_proc()));
        assert(array.get_layout() == Layout::Cyclic);

        std::uint64_t total_phases = util::log_base_2(length * array.get_num_proc());
        std::uint64_t first_pass_phases = total_phases - util::log_base_2(length);
        bitonic_sorter<T>(locals.data(), length, increasing, first_pass_phases);
        array.switch_layout(Layout::Blocked);
        bitonic_sorter<T>(locals.data(), length, increasing);
    }

    /*
     * Unlike the sorter() function, we can't implement this recursively
     * because the all-to-all communication phase to be shared by both of the
     * recursive parallel_sorter circuits; simply making a recursive call would
     * result in each recursive call having its own all-to-all phase.
     */

    template <typename T>
    void parallel_sorter(ShardedArray<T>& array, bool increasing = true) {
        std::vector<T>& locals = array.get_locals();
        std::uint64_t local_length = locals.size();

        assert(util::is_power_of_two(local_length));
        assert(util::is_power_of_two(array.get_num_proc()));
        assert(local_length >= array.get_num_proc());

        WorkerID k = array.get_self_id();

        /* First, do a fast local sort. */
        array.switch_layout(Layout::Blocked);
        sorter(locals.data(), local_length, util::hamming_parity(k) != increasing);

        /*
         * Terminology: The sorting circuit is composed of a series of merge
         * stages. Each merge stage is consists of a cyclic phase followed by
         * a blocked phase. Some stages only have a blocked phase.
         */

        std::uint64_t num_merge_stages = util::log_base_2(local_length * array.get_num_proc());
        for (std::uint64_t merge_stage = util::log_base_2(local_length); merge_stage < num_merge_stages; merge_stage++) {
            std::uint64_t merge_stage_depth = merge_stage + 1;
            std::uint64_t blocked_phase_depth = util::log_base_2(local_length);

            /* Cyclic phase */
            array.switch_layout(Layout::Cyclic);
            std::uint64_t cyclic_phase_depth = merge_stage_depth - blocked_phase_depth;
            std::uint64_t cyclic_array_length = UINT64_C(1) << (merge_stage_depth - util::log_base_2(array.get_num_proc()));
            unsigned int j = 0;
            for (std::uint64_t i = 0; i != local_length; i += cyclic_array_length) {
                /*
                 * To determine the direction, we count the number of set bits
                 * in the two's complement representation of j. If it's even,
                 * we sort this subarray in the same direction as we're sorting
                 * the overall array. If it's odd, then we sort this subarray
                 * in the opposite direction as the overall array.
                 */
                bool direction = util::hamming_parity(j++) != increasing;
                bitonic_sorter<T>(locals.data() + i, cyclic_array_length, direction, cyclic_phase_depth);
            }

            /* Blocked phase */
            array.switch_layout(Layout::Blocked);
            k >>= 1;
            bitonic_sorter<T>(locals.data(), local_length, util::hamming_parity(k) != increasing);
        }
    }
}

#endif
