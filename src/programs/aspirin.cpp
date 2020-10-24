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

namespace mage::programs::aspirin {
    template <BitWidth bits>
    struct Input {
        Integer<bits> patient_id_concat_timestamp;
        Bit diagnosis; // or aspirin prescription

        static void comparator(Input<bits>& arg0, Input<bits>& arg1) {
            Bit predicate = arg0.patient_id_concat_timestamp > arg1.patient_id_concat_timestamp;
            Integer<bits>::swap_if(predicate, arg0.patient_id_concat_timestamp, arg1.patient_id_concat_timestamp);
            Bit::swap_if(predicate, arg0.diagnosis, arg1.diagnosis);
        }

        void buffer_send(WorkerID to) {
            this->patient_id_concat_timestamp.buffer_send(to);
            this->diagnosis.buffer_send(to);
        }

        static void finish_send(WorkerID to) {
            Integer<bits>::finish_send(to);
        }

        void post_receive(WorkerID from) {
            this->patient_id_concat_timestamp.post_receive(from);
            this->diagnosis.post_receive(from);
        }

        static void finish_receive(WorkerID from) {
            Integer<bits>::finish_receive(from);
        }
    };

    template <BitWidth patient_id_bits = 32, BitWidth timestamp_bits = 32, BitWidth result_bits = 32>
    void create_parallel_aspirin_circuit(const ProgramOptions& args) {
        int input_array_length = args.problem_size * 2;

        ClusterUtils utils;
        utils.self_id = args.worker_index;
        utils.num_proc = args.num_workers;

        ShardedArray<Input<patient_id_bits + timestamp_bits>> inputs(input_array_length, args.worker_index, args.num_workers, Layout::Cyclic);
        inputs.for_each([=](std::size_t i, auto& input) {
            input.patient_id_concat_timestamp.mark_input(i < args.problem_size ? Party::Garbler : Party::Evaluator);
            input.diagnosis.mark_input(i < args.problem_size ? Party::Garbler : Party::Evaluator);
        });

        // Verify that inputs are sorted

        Bit local_order(1);
        inputs.for_each_pair([&](std::size_t i, auto& first, auto& second) {
            if (i < args.problem_size - 1) {
                Bit lte = first.patient_id_concat_timestamp <= second.patient_id_concat_timestamp;
                local_order = local_order & lte;
            } else if (i >= args.problem_size) {
                Bit gte = first.patient_id_concat_timestamp >= second.patient_id_concat_timestamp;
                local_order = local_order & gte;
            }
        });
        std::optional<Bit> order = utils.reduce_aggregates<Bit>(0, local_order, [](Bit& first, Bit& second) -> Bit {
            return first & second;
        });
        if (args.worker_index == 0) {
            order.value().mark_output();
        }

        // Sort inputs and switch to blocked layout
        parallel_bitonic_sorter(inputs);

        Integer<result_bits> local_total(0);
        inputs.for_each_pair([&local_total](std::size_t index, Input<patient_id_bits + timestamp_bits>& first, Input<patient_id_bits + timestamp_bits>& second) {
            Bit add = first.diagnosis & ~second.diagnosis;
            IntSlice<patient_id_bits> patient_id_i = first.patient_id_concat_timestamp.template slice<patient_id_bits>(timestamp_bits);
            IntSlice<patient_id_bits> patient_id_ip1 = second.patient_id_concat_timestamp.template slice<patient_id_bits>(timestamp_bits);
            add = add & (patient_id_i == patient_id_ip1);
            Integer<result_bits> next = local_total.increment();
            local_total = Integer<result_bits>::select(add, next, local_total);
        });

        std::optional<Integer<result_bits>> total = utils.reduce_aggregates<Integer<result_bits>>(0, local_total, [](Integer<result_bits>& first, Integer<result_bits>& second) -> Integer<result_bits> {
            return first + second;
        });
        if (args.worker_index == 0) {
            total.value().mark_output();
        }
    }

    RegisterProgram aspirin("aspirin", "Aspirin Count (problem_size = number of events per party)", create_parallel_aspirin_circuit<>);
}
