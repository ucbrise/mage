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

#include <cstdint>
#include "dsl/array.hpp"
#include "dsl/integer.hpp"
#include "dsl/parallel.hpp"
#include "dsl/sort.hpp"
#include "programs/registry.hpp"
#include "programs/util.hpp"

using namespace mage::dsl;

namespace mage::programs::real_matrix_vector_multiply {
    template <std::int32_t level = 0>
    std::vector<LeveledBatch<level, true>> local_matrix_vector_multiply(LeveledBatch<level + 1, true>* matrix_a, std::size_t num_rows_a, LeveledBatch<level + 1, true>* vector_x, std::size_t num_cols_a_len_x) {
        std::vector<LeveledBatch<level, true>> result(num_rows_a);
        for (std::size_t row_a = level; row_a != num_rows_a; row_a++) {
            result[row_a] = real_dot_product<level>(&matrix_a[row_a * num_cols_a_len_x], vector_x, num_cols_a_len_x);
        }
        return result;
    }

    template <std::int32_t level>
    void create_real_matrix_vector_multiply_circuit(const ProgramOptions& args) {
        std::uint64_t vector_size = args.problem_size;
        std::uint64_t matrix_dimension = vector_size;
        std::uint64_t matrix_size = matrix_dimension * matrix_dimension;

        program_ptr->print_stats();
        program_ptr->start_timer();

        /* Blocked vector provided by the evaluator. */
        ShardedArray<LeveledBatch<level + 1, true>> vector_x(vector_size, args.worker_index, args.num_workers, Layout::Blocked);
        vector_x.for_each([=](std::size_t i, auto& elem) {
            elem.mark_input();
        });

        /* Blocked row-major matrix provided by the garbler. */
        std::vector<LeveledBatch<level + 1, true>> my_matrix_a(vector_x.get_locals().size() * matrix_dimension);
        for (auto& elem : my_matrix_a) {
            elem.mark_input();
        }

        /* Reconstruct the entire vector x for each worker. */
        std::vector<LeveledBatch<level + 1, true>> my_vector_x = vector_x.materialize_global_array(true);

        /* Multiply my portion of the matrix by the entire vector. */
        std::vector<LeveledBatch<level, true>> result = local_matrix_vector_multiply<level>(my_matrix_a.data(), my_matrix_a.size() / my_vector_x.size(), my_vector_x.data(), my_vector_x.size());

        program_ptr->stop_timer();
        program_ptr->print_stats();

        for (std::size_t i = 0; i != result.size(); i++) {
            result[i].mark_output();
        }
    }

    RegisterProgram real_matrix_vector_multiply("real_matrix_vector_multiply", "Matrix-Vector Multiply with real numbers (problem_size = number of elements in one side of matrix)", create_real_matrix_vector_multiply_circuit<0>);
}
