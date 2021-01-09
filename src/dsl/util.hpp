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

/**
 * @file dsl/util.hpp
 * @brief Utility functions to use with MAGE's DSLs.
 */

#ifndef MAGE_DSL_UTIL_HPP_
#define MAGE_DSL_UTIL_HPP_

#include <cstdlib>
#include "instruction.hpp"
#include "dsl/integer.hpp"
#include "util/misc.hpp"

namespace mage::dsl {
    /**
     * @brief Computes the sum of a set of Integers, increasing the width as
     * needed to prevent overflow.
     *
     * @tparam output_bits The width of the sum, in bits.
     * @tparam bits The width of the Integers to add, in bits.
     * @tparam sliced True if the Integers to add are sliced, otherwise false.
     * @tparam Placer Type of placement module used by the Integers.
     * @tparam p Double pointer to the program object used by the Integers.
     * @param elements Pointer to the array of Integers whose sum to compute.
     * @param num_elements Length of the array whose sum to compute.
     * @return The sum, at the specified width.
     */
    template <BitWidth output_bits, BitWidth bits, bool sliced, typename Placer, Program<Placer>** p>
    Integer<output_bits, false, Placer, p> reduce(Integer<bits, sliced, Placer, p>* elements, std::size_t num_elements) {
        if constexpr (bits == output_bits) { // we need this to end the template recursion
            assert(num_elements == 1);
            Integer<output_bits, false, Placer, p> result;
            result.mutate(*elements);
            elements->recycle();
            return result;
        } else {
            if (num_elements == 1) {
                Integer<output_bits, false, Placer, p> result;
                result.mutate(*elements);
                elements->recycle();
                return result;
            }
            std::vector<Integer<bits + 1, false, Placer, p>> partial_sums(util::ceil_div(num_elements, 2).first);
            for (std::size_t i = 0; i < num_elements; i += 2) {
                if (i + 1 < num_elements) {
                    partial_sums[i >> 1] = elements[i].add_with_carry(elements[i + 1]);
                    elements[i].recycle();
                    elements[i + 1].recycle();
                } else {
                    partial_sums[i >> 1].mutate(elements[i]);
                    elements[i].recycle();
                }
            }
            return reduce<output_bits, bits + 1, false, Placer, p>(partial_sums.data(), partial_sums.size());
        }
    }

    /**
     * @brief Sends any buffered data to the destination workers, and blocks
     * until any outstanding receive operations complete.
     *
     * It is expected that all workers call this function concurrently.
     *
     * @param self_id The ID of the worker in whose program this is being
     * called.
     * @param num_proc The total number of workers.
     */
    template <typename T>
    void communication_barrier(WorkerID self_id, WorkerID num_proc) {
        for (WorkerID w = 0; w != num_proc; w++) {
            if (w != self_id) {
                T::finish_send(w);
            }
        }
        for (WorkerID w = 0; w != num_proc; w++) {
            if (w != self_id) {
                T::finish_receive(w);
            }
        }
    }
}

#endif
