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

#include <functional>
#include <vector>
#include "dsl/array.hpp"
#include "dsl/integer.hpp"
#include "dsl/parallel.hpp"
#include "dsl/sort.hpp"
#include "programs/registry.hpp"
#include "programs/util.hpp"

using namespace mage::dsl;

namespace mage::programs::loop_join {
    template <BitWidth t1_key_width = 32, BitWidth t1_record_width = 128, BitWidth t2_key_width = 32, BitWidth t2_record_width = 128>
    struct JoinedRecords {
        Bit valid;
        Record<t1_key_width, t1_record_width> t1_record;
        Record<t2_key_width, t2_record_width> t2_record;
    };

    template <BitWidth t1_key_width = 32, BitWidth t1_record_width = 128, BitWidth t2_key_width = 32, BitWidth t2_record_width = 128>
    std::vector<JoinedRecords<t1_key_width, t1_record_width, t2_key_width, t2_record_width>>
    local_naive_loop_join(const ProgramOptions& args,
                      std::vector<Record<t1_key_width, t1_record_width>>& table1,
                      std::vector<Record<t2_key_width, t2_record_width>>& table2,
                      std::function<Bit(IntSlice<t1_key_width>, IntSlice<t2_key_width>)> predicate) {
        std::vector<JoinedRecords<t1_key_width, t1_record_width, t2_key_width, t2_record_width>> joined(table1.size() * table2.size());

        Integer<t1_record_width> zero1(0);
        Integer<t2_record_width> zero2(0);

        for (std::size_t i = 0; i != table1.size(); i++) {
            for (std::size_t j = 0; j != table2.size(); j++) {
                /* Do the join. */
                std::size_t result_index = (i * table2.size()) + j;
                auto& result = joined[result_index];
                result.valid = predicate(table1[i].get_key(), table2[j].get_key());
                result.t1_record.data.mutate(table1[i].data);//Integer<t1_record_width>::select(result.valid, table1[i].data, zero1);
                result.t2_record.data.mutate(table2[j].data);//Integer<t2_record_width>::select(result.valid, table2[j].data, zero2);
            }
        }

        return joined;
    }

    template <BitWidth key_width = 32, BitWidth record_width = 128>
    void create_loop_join_circuit(const ProgramOptions& args) {
        int input_array_length = args.problem_size * 2;

        ClusterUtils utils;
        utils.self_id = args.worker_index;
        utils.num_proc = args.num_workers;

        ShardedArray<Record<key_width, record_width>> inputs(input_array_length, args.worker_index, args.num_workers, Layout::Cyclic);
        inputs.for_each([=](std::size_t i, auto& input) {
            input.data.mark_input(i < args.problem_size ? Party::Garbler : Party::Evaluator);
        });

        std::vector<Record<key_width, record_width>> my_table1;
        std::vector<Record<key_width, record_width>> my_table2;

        {
            std::vector<Record<key_width, record_width>>& locals = inputs.get_locals();

            assert(util::is_power_of_two(args.num_workers));
            std::uint8_t log_num_workers = util::log_base_2(args.num_workers);

            std::uint32_t num_partitions_per_table = args.num_workers / 2;

            std::uint32_t num_portions_table1 = UINT32_C(1) << ((log_num_workers / 2) + (log_num_workers % 2));
            std::uint32_t num_portions_table2 = UINT32_C(1) << (log_num_workers / 2);

            std::uint32_t portion_size_table1 = args.problem_size / num_portions_table1;
            std::uint32_t portion_size_table2 = args.problem_size / num_portions_table2;

            std::uint32_t my_table1_portion = args.worker_index / num_portions_table2;
            std::uint32_t my_table2_portion = args.worker_index % num_portions_table2;
            my_table1.resize(portion_size_table1);
            my_table2.resize(portion_size_table2);

            auto [ base, stride ] = inputs.get_global_base_and_stride();
            for (std::size_t i = 0; i != input_array_length; i++) {
                /* Find the portion and table corresponding to global index i. */
                bool table1 = (i < portion_size_table1 * num_portions_table1);
                std::size_t i_portion, i_portion_index;
                if (table1) {
                    i_portion = i / portion_size_table1;
                    i_portion_index = i % portion_size_table1;
                } else {
                    i_portion = (i - portion_size_table1 * num_portions_table1) / portion_size_table2;
                    i_portion_index = (i - portion_size_table1 * num_portions_table1) % portion_size_table2;
                }

                WorkerID who = inputs.who(i);
                std::size_t j = (i - base) / stride;
                if (who == args.worker_index) {
                    /* Figure out who needs the item with global index i; we have it at local index j. */
                    if (table1) {
                        /* This goes to a continuous block of workers. */
                        bool send_to_self = false;
                        WorkerID to_start = i_portion * num_portions_table2;
                        for (WorkerID to = to_start; to != to_start + num_portions_table2; to++) {
                            // std::cout << "Table 1, " << i << " -> " << to << std::endl;
                            if (to == args.worker_index) {
                                send_to_self = true;
                            } else {
                                locals[j].buffer_send(to);
                            }
                        }
                        if (send_to_self) {
                            /* We can't do this inside the above loop because of std:move. */
                            // std::cout << "Table 1, " << i_portion_index << " <- " << args.worker_index << std::endl;
                            my_table1[i_portion_index] = std::move(locals[j]);
                        }
                    } else {
                        /* This goes to a strided set of workers. */
                        bool send_to_self = false;
                        WorkerID to_start = i_portion;
                        for (WorkerID to = to_start; to < args.num_workers; to += num_portions_table2) {
                            // std::cout << "Table 2, " << i << " -> " << to << std::endl;
                            if (to == args.worker_index) {
                                send_to_self = true;
                            } else {
                                locals[j].buffer_send(to);
                            }
                        }
                        if (send_to_self) {
                            /* We can't do this inside the above loop because of std:move. */
                            // std::cout << "Table 2, " << i_portion_index << " <- " << args.worker_index << std::endl;
                            my_table2[i_portion_index] = std::move(locals[j]); // BUG: future buffer_sends on locals[j] won't work anymore
                        }
                    }
                } else {
                    /* Check if we need to receive the item with global index i from someone else. */
                    if (table1 && i_portion == my_table1_portion) {
                        // std::cout << "Table 1, " << i_portion_index << " <- " << who << std::endl;
                        my_table1[i_portion_index].post_receive(who);
                    } else if (!table1 && i_portion == my_table2_portion) {
                        // std::cout << "Table 2, " << i_portion_index << " <- " << who << std::endl;
                        my_table2[i_portion_index].post_receive(who);
                    }
                }
            }
        }

        for (WorkerID w = 0; w != args.num_workers; w++) {
            if (w != args.worker_index) {
                Bit::finish_send(w);
            }
        }
        for (WorkerID w = 0; w != args.num_workers; w++) {
            if (w != args.worker_index) {
                Bit::finish_receive(w);
            }
        }

        std::vector<JoinedRecords<key_width, record_width, key_width, record_width>> joined = local_naive_loop_join<key_width, record_width, key_width, record_width>(args, my_table1, my_table2, [](IntSlice<key_width> key1, IntSlice<key_width> key2) -> Bit {
            return key1 < key2;
        });

        for (int i = 0; i != joined.size(); i++) {
            joined[i].valid.mark_output();
            joined[i].t1_record.data.mark_output();
            joined[i].t2_record.data.mark_output();
        }
    }

    RegisterProgram loop_join("loop_join", "Join two tables on non-equality condition (problem_size = number of records per party)", create_loop_join_circuit<>);
}
