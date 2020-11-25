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

namespace mage::programs::real_sum {
    void create_real_sum_circuit(const ProgramOptions& args) {
        int input_array_length = args.problem_size;

        ClusterUtils utils;
        utils.self_id = args.worker_index;
        utils.num_proc = args.num_workers;

        ShardedArray<LeveledBatch<0>> inputs(input_array_length, args.worker_index, args.num_workers, Layout::Blocked);
        inputs.for_each([=](std::size_t i, auto& input) {
            input.mark_input();
        });


        std::vector<LeveledBatch<0>>& locals = inputs.get_locals();

        LeveledBatch<0> local_result;
        if (locals.size() == 0) {
            local_result = LeveledBatch<0>(0);
        } else {
            local_result = std::move(locals[0]);
            for (std::size_t i = 1; i != locals.size(); i++) {
                local_result = local_result + locals[i];
            }
        }

        std::optional<LeveledBatch<0>> global_result = utils.reduce_aggregates<LeveledBatch<0>>(0, local_result, [](LeveledBatch<0>& a, LeveledBatch<0>& b) -> LeveledBatch<0> {
            return a + b;
        });

        if (args.worker_index == 0) {
            global_result->mark_output();
        }
    }

    RegisterProgram real_sum("real_sum", "Compute sum of array of real numbers (problem_size = number of elements)", create_real_sum_circuit);
}
