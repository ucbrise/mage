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

#include "dsl/array.hpp"
#include "dsl/integer.hpp"
#include "dsl/parallel.hpp"
#include "dsl/sort.hpp"
#include "programs/registry.hpp"
#include "programs/util.hpp"

using namespace mage::dsl;

namespace mage::programs::merge_sorted {
    template <BitWidth key_width = 32, BitWidth record_width = 128>
    void create_merge_sorted_circuit(const ProgramOptions& args) {
        int input_array_length = args.problem_size * 2;

        mage::dsl::ClusterUtils utils;
        utils.self_id = args.worker_index;
        utils.num_proc = args.num_workers;

        ShardedArray<Record<key_width, record_width>> inputs(input_array_length, args.worker_index, args.num_workers, Layout::Cyclic);
        inputs.for_each([=](std::size_t i, auto& input) {
            input.data.mark_input(i < args.problem_size ? Party::Garbler : Party::Evaluator);
        });

        // Verify that inputs are sorted
        Bit local_order(1);
        inputs.for_each_pair([&](std::size_t i, auto& first, auto& second) {
            if (i < args.problem_size - 1) {
                Bit lte = first.get_key() <= second.get_key();
                local_order = local_order & lte;
            } else if (i >= args.problem_size) {
                Bit gte = first.get_key() >= second.get_key();
                local_order = local_order & gte;
            }
        });
        std::optional<Bit> order = utils.reduce_aggregates<Bit>(0, local_order, [](Bit& first, Bit& second) -> Bit {
            return first & second;
        });
        if (args.worker_index == 0) {
            order.value().mark_output();
        }

        // Sort inputs and switch to blocked layout
        mage::dsl::parallel_bitonic_sorter(inputs);

        // Output sorted list
        inputs.for_each([=](std::size_t i, auto& input) {
            input.data.mark_output();
        });
    }

    RegisterProgram merge_sorted("merge_sorted", "Merge Sorted Lists (problem_size = number of elements per party)", create_merge_sorted_circuit<>);
}
