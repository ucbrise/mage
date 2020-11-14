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

namespace mage::programs::matrix_multiply {
    template <BitWidth width = 8>
    std::vector<Integer<2 * width>> local_naive_matrix_multiply(Integer<width>* matrix_a, std::size_t num_rows_a, Integer<width>* matrix_b, std::size_t num_cols_b, std::size_t num_cols_a_rows_b) {
        std::vector<Integer<2 * width>> result(num_rows_a * num_cols_b);
        for (std::size_t row_a = 0; row_a != num_rows_a; row_a++) {
            for (std::size_t col_b = 0; col_b != num_cols_b; col_b++) {
                /* This goes in result at row row_a and column col_b. */
                std::size_t i = row_a * num_cols_b + col_b;
                result[i] = dot_product(&matrix_a[row_a * num_cols_a_rows_b], &matrix_b[col_b * num_cols_a_rows_b], num_cols_a_rows_b);
            }
        }
        return result;
    }

    template <BitWidth width = 8>
    void create_matrix_multiply_circuit(const ProgramOptions& args) {
        int matrix_dimension = args.problem_size;
        int matrix_size = matrix_dimension * matrix_dimension;

        /* Blocked row-major matrix provided by the garbler. */
        ShardedArray<Integer<width>> matrix_a(matrix_size, args.worker_index, args.num_workers, Layout::Blocked);
        matrix_a.for_each([=](std::size_t i, auto& elem) {
            elem.mark_input(Party::Garbler);
        });

        /* Blocked column-major matrix provided by the evaluator. */
        ShardedArray<Integer<width>> matrix_b(matrix_size, args.worker_index, args.num_workers, Layout::Blocked);
        matrix_b.for_each([=](std::size_t i, auto& elem) {
            elem.mark_input(Party::Evaluator);
        });

        ClusterUtils utils;
        utils.self_id = args.worker_index;
        utils.num_proc = args.num_workers;
        auto [ my_matrix_a, my_matrix_b ] = utils.cross_product(matrix_a, matrix_b);

        std::vector<Integer<2 * width>> result = local_naive_matrix_multiply(my_matrix_a.data(), my_matrix_a.size() / matrix_dimension, my_matrix_b.data(), my_matrix_b.size() / matrix_dimension, matrix_dimension);
        for (std::size_t i = 0; i != result.size(); i++) {
            result[i].mark_output();
        }
    }

    RegisterProgram matrix_multiply("matrix_multiply", "Matrix Multiply (problem_size = number of elements in one side of matrix)", create_matrix_multiply_circuit<>);
}
