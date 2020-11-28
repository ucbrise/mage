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
    struct Stats {
        LeveledBatch<2, true> sum;
        LeveledBatch<1, true> sum_squares;

        void buffer_send(WorkerID to) {
            this->sum.buffer_send(to);
            this->sum_squares.buffer_send(to);
        }

        static void finish_send(WorkerID to) {
            LeveledBatch<0, true>::finish_send(to);
        }

        void post_receive(WorkerID from) {
            this->sum.post_receive(from);
            this->sum_squares.post_receive(from);
        }

        static void finish_receive(WorkerID from) {
            LeveledBatch<0, true>::finish_receive(from);
        }
    };

    void create_real_statistics_circuit(const ProgramOptions& args) {
        int input_array_length = args.problem_size;

        ClusterUtils utils;
        utils.self_id = args.worker_index;
        utils.num_proc = args.num_workers;

        ShardedArray<LeveledBatch<2, true>> inputs(input_array_length, args.worker_index, args.num_workers, Layout::Blocked);
        inputs.for_each([=](std::size_t i, auto& input) {
            input.mark_input();
        });

        program_ptr->print_stats();
        program_ptr->start_timer();

        std::vector<LeveledBatch<2, true>>& locals = inputs.get_locals();

        Stats local;
        if (locals.size() == 0) {
            local.sum = LeveledBatch<2, true>(0);
            local.sum_squares = LeveledBatch<1, true>(0);
        } else {
            LeveledBatch<2, false> temp = locals[0].multiply_without_normalizing(locals[0]);
            local.sum = std::move(locals[0]);
            for (std::size_t i = 1; i != locals.size(); i++) {
                temp = temp + locals[i].multiply_without_normalizing(locals[i]);
                local.sum = local.sum + locals[i];
            }
            local.sum_squares = temp.renormalize();
        }

        std::optional<Stats> global_stats = utils.reduce_aggregates<Stats>(0, local, [](Stats& a, Stats& b) -> Stats {
            Stats result;
            result.sum = a.sum + b.sum;
            result.sum_squares = a.sum_squares + b.sum_squares;
            return result;
        });

        if (args.worker_index == 0) {
            LeveledBatch<1, true> mean = global_stats->sum * LeveledPlaintextBatch<2>(1 / static_cast<double>(input_array_length));
            LeveledBatch<0, true> mean_squares = global_stats->sum_squares * LeveledPlaintextBatch<1>(1 / static_cast<double>(input_array_length));
            LeveledBatch<0, true> variance = mean_squares - (mean * mean);

            program_ptr->stop_timer();
            program_ptr->print_stats();

            mean.mark_output();
            variance.mark_output();
        } else {
            program_ptr->stop_timer();
            program_ptr->print_stats();
        }
    }

    RegisterProgram real_statistics("real_statistics", "Compute mean and variance of real numbers (problem_size = number of elements)", create_real_statistics_circuit);
}
