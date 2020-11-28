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

namespace mage::programs::binary_fc_layer {
    template <BitWidth batch_size>
    Bit binary_dot_product(Integer<batch_size>* vector_a, Integer<batch_size>* vector_b, std::size_t num_batches) {
        assert(num_batches != 0);
        std::vector<Integer<batch_size>> xnors(num_batches);
        std::vector<BitSlice> xnor_bits;
        for (std::size_t i = 0; i != num_batches; i++) {
            xnors[i] = ~(vector_a[i] ^ vector_b[i]);
            for (BitWidth j = 0; j != batch_size; j++) {
                xnor_bits.emplace_back(xnors[i][j]);
            }
        }

        Integer<31> popcount = reduce<31, 1, true, DefaultPlacer, default_program>(xnor_bits.data(), xnor_bits.size());
        Integer<32> two_p(0);
        two_p.slice<31>(1).mutate(popcount);

        return two_p >= Integer<32>(xnor_bits.size());
    }

    template <BitWidth batch_size>
    std::vector<Bit> local_binary_layer(Integer<batch_size>* matrix_a, std::size_t num_rows_a, Integer<batch_size>* vector_x, std::size_t num_cols_a_len_x) {
        std::vector<Bit> result(num_rows_a);
        for (std::size_t row_a = 0; row_a != num_rows_a; row_a++) {
            result[row_a] = binary_dot_product<batch_size>(&matrix_a[row_a * num_cols_a_len_x], vector_x, num_cols_a_len_x);
        }
        return result;
    }

    template <BitWidth batch_size = 256>
    void create_binary_fc_layer_circuit(const ProgramOptions& args) {
        std::uint64_t vector_size = args.problem_size;
        std::uint64_t matrix_dimension = vector_size;
        std::uint64_t matrix_size = matrix_dimension * matrix_dimension;

        if (vector_size % batch_size != 0) {
            std::cerr << "Problem size must be a multiple of the batch size" << std::endl;
            return;
        }

        /* Blocked vector provided by the evaluator. */
        ShardedArray<Integer<batch_size>> vector_x(vector_size / batch_size, args.worker_index, args.num_workers, Layout::Blocked);
        vector_x.for_each([=](std::size_t i, auto& elem) {
            elem.mark_input(Party::Evaluator);
        });

        program_ptr->print_stats();
        program_ptr->start_timer();


        /* Blocked row-major matrix provided by the garbler. */
        std::uint64_t num_columns = matrix_dimension / batch_size;
        std::uint64_t num_rows = matrix_dimension / args.num_workers;
        if (args.worker_index < matrix_dimension % args.num_workers) {
            num_rows += 1;
        }
        std::vector<Integer<batch_size>> my_matrix_a(num_rows * num_columns);
        for (auto& elem : my_matrix_a) {
            elem.mark_input(Party::Garbler);
        }

        /* Reconstruct the entire vector x for each worker. */
        std::vector<Integer<batch_size>> my_vector_x = vector_x.materialize_global_array(true);

        std::vector<Bit> result = local_binary_layer<batch_size>(my_matrix_a.data(), my_matrix_a.size() / my_vector_x.size(), my_vector_x.data(), my_vector_x.size());

        program_ptr->stop_timer();
        program_ptr->print_stats();

        for (std::size_t i = 0; i != result.size(); i++) {
            result[i].mark_output();
        }
    }

    RegisterProgram binary_fc_layer("binary_fc_layer", "Binary Matrix-Vector Multiply (problem_size = number of elements in one side of matrix)", create_binary_fc_layer_circuit<>);
}
