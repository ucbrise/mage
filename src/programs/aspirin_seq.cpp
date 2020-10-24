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

namespace mage::programs::aspirin_seq {
    template <BitWidth bits>
    struct Input {
        Integer<bits> patient_id_concat_timestamp;
        Bit diagnosis; // or aspirin prescription

        static void comparator(Input<bits>& arg0, Input<bits>& arg1) {
            Bit predicate = arg0.patient_id_concat_timestamp > arg1.patient_id_concat_timestamp;
            Integer<bits>::swap_if(predicate, arg0.patient_id_concat_timestamp, arg1.patient_id_concat_timestamp);
            Bit::swap_if(predicate, arg0.diagnosis, arg1.diagnosis);
        }
    };

    template <BitWidth patient_id_bits = 32, BitWidth timestamp_bits = 32, BitWidth result_bits = 32>
    void create_aspirin_circuit(const ProgramOptions& args) {
        int input_array_length = args.problem_size * 2;
        std::vector<Input<patient_id_bits + timestamp_bits>> inputs;

        for (int i = 0; i != input_array_length; i++) {
            inputs.emplace_back();
            inputs[i].patient_id_concat_timestamp.mark_input(i < args.problem_size ? Party::Garbler : Party::Evaluator);
            inputs[i].diagnosis.mark_input(i < args.problem_size ? Party::Garbler : Party::Evaluator);
        }

        // Verify the input first.
        Bit order(1);
        for (int i = 0; i < args.problem_size - 1; i++) {
            Bit lte = inputs[i].patient_id_concat_timestamp <= inputs[i+1].patient_id_concat_timestamp;
            order = order & lte;
        }
        for (int i = args.problem_size; i < 2 * args.problem_size - 1; i++) {
            Bit gte = inputs[i].patient_id_concat_timestamp >= inputs[i+1].patient_id_concat_timestamp;
            order = order & gte;
        }
    	order.mark_output();

        // Merge the two arrays, sorted ascending by patient_id_concat_timestamp
        bitonic_sorter(inputs.data(), input_array_length);

        // Now, for each input, check if it and the next input have the same patient, but the first is a diagnosis and the second isn't.
        Integer<result_bits> total(0);
        for (int i = 0; i < input_array_length - 1; i++) {
            Bit add = inputs[i].diagnosis & ~inputs[i+1].diagnosis;
            IntSlice<patient_id_bits> patient_id_i = inputs[i].patient_id_concat_timestamp.template slice<patient_id_bits>(timestamp_bits);
            IntSlice<patient_id_bits> patient_id_ip1 = inputs[i+1].patient_id_concat_timestamp.template slice<patient_id_bits>(timestamp_bits);
            add = add & (patient_id_i == patient_id_ip1);
            Integer<result_bits> next = total.increment();
            total = Integer<result_bits>::select(add, next, total);
        }

        total.mark_output();
    }

    RegisterProgram aspirin_seq("aspirin_seq", "Aspirin Count where each worker computes the whole thing", create_aspirin_circuit<>);
}
