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

namespace mage::programs::real_matrix_multiply {
    template <std::int32_t level = 0>
    std::vector<LeveledBatch<level, true>> local_naive_matrix_multiply(LeveledBatch<level + 1, true>* matrix_a, std::size_t num_rows_a, LeveledBatch<level + 1, true>* matrix_b, std::size_t num_cols_b, std::size_t num_cols_a_rows_b) {
        std::vector<LeveledBatch<level, true>> result(num_rows_a * num_cols_b);
        for (std::size_t row_a = 0; row_a != num_rows_a; row_a++) {
            for (std::size_t col_b = 0; col_b != num_cols_b; col_b++) {
                /* This goes in result at row row_a and column col_b. */
                std::size_t i = row_a * num_cols_b + col_b;
                result[i] = real_dot_product<0>(&matrix_a[row_a * num_cols_a_rows_b], &matrix_b[col_b * num_cols_a_rows_b], num_cols_a_rows_b);
            }
        }
        return result;
    }

    template <std::int32_t level = 0>
    std::vector<LeveledBatch<level, true>> local_tiled_matrix_multiply(std::size_t batch_dimension, LeveledBatch<level + 1, true>* matrix_a, std::size_t num_rows_a, LeveledBatch<level + 1, true>* matrix_b, std::size_t num_cols_b, std::size_t num_cols_a_rows_b) {
        std::vector<LeveledBatch<level, true>> result(num_rows_a * num_cols_b);
        // for (std::size_t i = 0; i != num_rows_a * num_cols_b; i++) {
        //     result[i] = LeveledBatch<level, true>(0);
        // }
        std::size_t num_rows_a_batches = util::ceil_div(num_rows_a, batch_dimension).first;
        std::size_t num_cols_b_batches = util::ceil_div(num_cols_b, batch_dimension).first;
        std::size_t num_cols_a_rows_b_batches = util::ceil_div(num_cols_a_rows_b, batch_dimension).first;

        for (std::size_t batch_row_a = 0; batch_row_a < num_rows_a; batch_row_a += batch_dimension) {
            for (std::size_t batch_col_b = 0; batch_col_b < num_cols_b; batch_col_b += batch_dimension) {
                std::vector<LeveledBatch<level + 1, false>> result_batch(batch_dimension * batch_dimension);
                for (std::size_t batch_cols_a_rows_b = 0; batch_cols_a_rows_b < num_cols_a_rows_b; batch_cols_a_rows_b += batch_dimension) {
                    /* Multiply the submatrices. */
                    for (std::size_t row_a = batch_row_a; row_a < num_rows_a && row_a < batch_row_a + batch_dimension; row_a++) {
                        for (std::size_t col_b = batch_col_b; col_b < num_cols_b && col_b < batch_col_b + batch_dimension; col_b++) {
                            /* This goes in result at row row_a and column col_b. */
                            std::size_t i_batch = (row_a - batch_row_a) * batch_dimension + (col_b - batch_col_b);
                            std::size_t dot_product_size = std::min(batch_dimension, num_cols_a_rows_b - batch_cols_a_rows_b);
                            LeveledBatch<level + 1, false> dot_product_result = real_dot_product_not_normalized<level>(&matrix_a[row_a * num_cols_a_rows_b + batch_cols_a_rows_b], &matrix_b[col_b * num_cols_a_rows_b + batch_cols_a_rows_b], dot_product_size);
                            if (result_batch[i_batch].valid()) {
                                result_batch[i_batch] = result_batch[i_batch] + dot_product_result;
                            } else {
                                result_batch[i_batch] = std::move(dot_product_result);
                            }
                            if (batch_cols_a_rows_b + batch_dimension >= num_cols_a_rows_b) {
                                std::size_t i = row_a * num_cols_b + col_b;
                                result[i] = result_batch[i_batch].renormalize();
                                result_batch[i_batch].recycle();
                            }
                        }
                    }
                }
            }
        }
        return result;
    }

    template <std::int32_t level, bool tiled>
    void create_real_matrix_multiply_circuit(const ProgramOptions& args) {
        int matrix_dimension = args.problem_size;
        int matrix_size = matrix_dimension * matrix_dimension;

        /* Blocked row-major matrix provided by the garbler. */
        ShardedArray<LeveledBatch<level + 1, true>> matrix_a(matrix_size, args.worker_index, args.num_workers, Layout::Blocked);
        matrix_a.for_each([=](std::size_t i, auto& elem) {
            elem.mark_input();
        });

        /* Blocked column-major matrix provided by the evaluator. */
        ShardedArray<LeveledBatch<level + 1, true>> matrix_b(matrix_size, args.worker_index, args.num_workers, Layout::Blocked);
        matrix_b.for_each([=](std::size_t i, auto& elem) {
            elem.mark_input();
        });

        program_ptr->print_stats();
        program_ptr->start_timer();

        ClusterUtils utils;
        utils.self_id = args.worker_index;
        utils.num_proc = args.num_workers;
        auto [ my_matrix_a, my_matrix_b ] = utils.cross_product(matrix_a, matrix_b);

        std::vector<LeveledBatch<level, true>> result;
        if constexpr (tiled) {
            /* Hardcode tile size to work well for 1 GiB of memory. */
            std::size_t tile_dimension = 16;
            result = local_tiled_matrix_multiply(tile_dimension, my_matrix_a.data(), my_matrix_a.size() / matrix_dimension, my_matrix_b.data(), my_matrix_b.size() / matrix_dimension, matrix_dimension);
        } else {
            result = local_naive_matrix_multiply(my_matrix_a.data(), my_matrix_a.size() / matrix_dimension, my_matrix_b.data(), my_matrix_b.size() / matrix_dimension, matrix_dimension);
        }

        program_ptr->stop_timer();
        program_ptr->print_stats();

        for (std::size_t i = 0; i != result.size(); i++) {
            result[i].mark_output();
        }
    }

    RegisterProgram real_naive_matrix_multiply("real_naive_matrix_multiply", "Naive matrix multiply with real numbers (problem_size = number of elements in one side of matrix)", create_real_matrix_multiply_circuit<0, false>);
    RegisterProgram real_tiled_matrix_multiply("real_tiled_matrix_multiply", "Tiled matrix multiply with real numbers (problem_size = number of elements in one side of matrix)", create_real_matrix_multiply_circuit<0, true>);
}
