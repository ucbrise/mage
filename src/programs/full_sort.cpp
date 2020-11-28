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
    void create_full_sort_circuit(const ProgramOptions& args) {
        int input_array_length = args.problem_size * 2;

        ClusterUtils utils;
        utils.self_id = args.worker_index;
        utils.num_proc = args.num_workers;

        ShardedArray<Record<key_width, record_width>> list(input_array_length, args.worker_index, args.num_workers, Layout::Cyclic);
        list.for_each([=](std::size_t i, auto& elem) {
            elem.data.mark_input(i < args.problem_size ? Party::Garbler : Party::Evaluator);
        });

        program_ptr->print_stats();
        program_ptr->start_timer();

        parallel_sorter(list);

        program_ptr->stop_timer();
        program_ptr->print_stats();

        list.for_each([=](std::size_t i, auto& elem) {
            elem.data.mark_output();
        });
    }

    RegisterProgram full_sort("full_sort", "Bitonic Sort (problem_size = number of elements per party)", create_full_sort_circuit<>);
}
