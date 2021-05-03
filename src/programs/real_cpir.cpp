/*
 * Copyright (C) 2021 Sam Kumar <samkumar@cs.berkeley.edu>
 * Copyright (C) 2021 University of California, Berkeley
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

namespace mage::programs::real_cpir {
    void create_real_cpir_circuit(const ProgramOptions& args) {
        int num_sets = args.problem_size;
        int set_size = args.problem_size;
        int input_array_length = num_sets * set_size;

        ClusterUtils utils;
        utils.self_id = args.worker_index;
        utils.num_proc = args.num_workers;

        ShardedArray<LeveledPlaintextBatch<1>> inputs(input_array_length, args.worker_index, args.num_workers, Layout::Blocked);
        inputs.for_each([=](std::size_t i, auto& input) {
            input = LeveledPlaintextBatch<1>(static_cast<double>(i + 1));
        });

        program_ptr->print_stats();
        program_ptr->start_timer();

        std::vector<LeveledPlaintextBatch<1>>& locals = inputs.get_locals();

        if ((locals.size() % set_size) != 0) {
            std::cerr << "Each worker must handle a whole number of sets" << std::endl;
            std::abort();
        }

        std::vector<LeveledBatch<1, true>> set_request(set_size);
        for (int i = 0; i != set_size; i++) {
            set_request[i].mark_input();
        }

        for (std::size_t i = 0; i != locals.size(); i += set_size) {
            LeveledBatch<1, false> set_response = set_request[0].multiply_without_normalizing(locals[i]);
            for (std::size_t j = 1; j != set_size; j++) {
                set_response = set_response + set_request[j].multiply_without_normalizing(locals[i + j]);
            }
            set_response.renormalize().mark_output();
        }

        program_ptr->stop_timer();
        program_ptr->print_stats();
    }

    RegisterProgram real_cpir("real_cpir", "Perform computational PIR on an array of real numbers (problem_size = square root of the number of elements)", create_real_cpir_circuit);
}
