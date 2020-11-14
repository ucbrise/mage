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

namespace mage::programs::matrix_vector_multiply {
    template <BitWidth width = 8>
    std::vector<Integer<2 * width>> local_matrix_vector_multiply(Integer<width>* matrix_a, std::size_t num_rows_a, Integer<width>* vector_x, std::size_t num_cols_a_len_x) {
        std::vector<Integer<2 * width>> result(num_rows_a);
        for (std::size_t row_a = 0; row_a != num_rows_a; row_a++) {
            result[row_a] = dot_product(&matrix_a[row_a * num_cols_a_len_x], vector_x, num_cols_a_len_x);
        }
        return result;
    }

    template <BitWidth width = 8>
    void create_matrix_vector_multiply_circuit(const ProgramOptions& args) {
        std::uint64_t vector_size = args.problem_size;
        std::uint64_t matrix_dimension = vector_size;
        std::uint64_t matrix_size = matrix_dimension * matrix_dimension;

        /* Blocked vector provided by the evaluator. */
        ShardedArray<Integer<width>> vector_x(vector_size, args.worker_index, args.num_workers, Layout::Blocked);
        vector_x.for_each([=](std::size_t i, auto& elem) {
            elem.mark_input(Party::Evaluator);
        });
        std::vector<Integer<width>>& vector_x_locals = vector_x.get_locals();

        /* Blocked row-major matrix provided by the garbler. */
        std::vector<Integer<width>> my_matrix_a(vector_x_locals.size() * matrix_dimension);
        for (auto& elem : my_matrix_a) {
            elem.mark_input(Party::Garbler);
        }

        /* Reconstruct the entire vector x for each worker. */
        std::vector<Integer<width>> my_vector_x(vector_size);
        auto [ base_size, num_extras ] = util::floor_div(vector_size, args.num_workers);
        for (std::uint64_t i = 0; i != base_size + 1; i++) {
            if (i < vector_x_locals.size()) {
                for (WorkerID w = 0; w != args.num_workers; w++) {
                    if (w != args.worker_index) {
                        vector_x_locals[i].buffer_send(w);
                    }
                }
            }
            for (WorkerID w = 0; w != args.num_workers; w++) {
                std::uint64_t w_base = vector_x.get_global_base_and_stride(w, Layout::Blocked).first;
                if (i < base_size || w < num_extras) {
                    if (w == args.worker_index) {
                        my_vector_x[w_base + i] = std::move(vector_x_locals[i]);
                    } else {
                        my_vector_x[w_base + i].post_receive(w);
                    }
                }
            }
        }

        ClusterUtils utils;
        utils.self_id = args.worker_index;
        utils.num_proc = args.num_workers;
        utils.communication_barrier<Bit>();

        std::cout << my_matrix_a.size() << " " << my_vector_x.size() << std::endl;

        std::vector<Integer<2 * width>> result = local_matrix_vector_multiply(my_matrix_a.data(), my_matrix_a.size() / my_vector_x.size(), my_vector_x.data(), my_vector_x.size());
        for (std::size_t i = 0; i != result.size(); i++) {
            result[i].mark_output();
        }
    }

    RegisterProgram matrix_vector_multiply("matrix_vector_multiply", "Matrix-Vector Multiply (problem_size = number of elements in one side of matrix)", create_matrix_vector_multiply_circuit<>);
}
