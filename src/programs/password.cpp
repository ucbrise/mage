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

namespace mage::programs::password {
    template <BitWidth user_bits, BitWidth pw_bits>
    struct UserPassword {
        Integer<user_bits + pw_bits> value;

        IntSlice<user_bits> get_user_id() {
            return this->value.template slice<user_bits>(pw_bits);
        }

        IntSlice<pw_bits> get_pw_hash() {
            return this->value.template slice<pw_bits>(0);
        }

        static void comparator(UserPassword<user_bits, pw_bits>& arg0, UserPassword<user_bits, pw_bits>& arg1) {
            Bit predicate = arg0.get_user_id() > arg1.get_user_id();
            Integer<user_bits + pw_bits>::swap_if(predicate, arg0.value, arg1.value);
        }

        void buffer_send(WorkerID to) {
            this->value.buffer_send(to);
        }

        static void finish_send(WorkerID to) {
            Integer<user_bits + pw_bits>::finish_send(to);
        }

        void post_receive(WorkerID from) {
            this->value.post_receive(from);
        }

        static void finish_receive(WorkerID from) {
            Integer<user_bits + pw_bits>::finish_receive(from);
        }
    };

    template <BitWidth user_bits = 32, BitWidth pw_bits = 256>
    void create_password_circuit(const ProgramOptions& args) {
        int input_array_length = args.problem_size * 2;

        ClusterUtils utils;
        utils.self_id = args.worker_index;
        utils.num_proc = args.num_workers;

        ShardedArray<UserPassword<user_bits, pw_bits>> inputs(input_array_length, args.worker_index, args.num_workers, Layout::Cyclic);
        inputs.for_each([=](std::size_t i, auto& input) {
            input.value.mark_input(i < args.problem_size ? Party::Garbler : Party::Evaluator);
        });

        /* Skip verifying the inputs, since this is semi-honest MPC. */

        /* Merge the two sorted arrays, sorted by user but not password. */
        parallel_bitonic_sorter(inputs);

        /* Do the PSI. */
        Integer<user_bits> zero, output;
        zero.mutate_to_constant(0);

        inputs.for_each_pair([&](std::size_t i, auto& a, auto& b) {
            Bit equals = (a.value == b.value);
            output.select(equals, a.get_user_id(), zero);
            output.mark_output();
        });
    }

    RegisterProgram password("password", "Password reuse query from the Senate paper", create_password_circuit<>);
}
