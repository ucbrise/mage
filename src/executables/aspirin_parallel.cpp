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

#include <cstdlib>
#include <cstring>
#include <chrono>
#include <iostream>
#include <string>
#include <utility>
#include "dsl/array.hpp"
#include "dsl/integer.hpp"
#include "dsl/parallel.hpp"
#include "dsl/sort.hpp"
#include "memprog/pipeline.hpp"
#include "programfile.hpp"
#include "util/config.hpp"

using mage::BitWidth;
using mage::WorkerID;
using mage::dsl::Party;
using mage::dsl::ShardedArray;
using mage::dsl::Layout;

mage::memprog::DefaultProgram* p;

template <BitWidth bits>
using Integer = mage::dsl::Integer<bits, false, mage::memprog::BinnedPlacer, &p>;

template <BitWidth bits>
using IntSlice = mage::dsl::Integer<bits, true, mage::memprog::BinnedPlacer, &p>;

using Bit = Integer<1>;
using BitSlice = IntSlice<1>;

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
void create_parallel_aspirin_circuit(mage::WorkerID index, mage::WorkerID num_workers, int input_size_per_party) {
    int input_array_length = input_size_per_party * 2;

    mage::dsl::ClusterUtils utils;
    utils.self_id = index;
    utils.num_proc = num_workers;

    ShardedArray<Input<patient_id_bits + timestamp_bits>> inputs(input_array_length, index, num_workers, Layout::Cyclic);
    inputs.for_each([=](std::size_t i, auto& input) {
        input.patient_id_concat_timestamp.mark_input(i < input_size_per_party ? Party::Garbler : Party::Evaluator);
        input.diagnosis.mark_input(i < input_size_per_party ? Party::Garbler : Party::Evaluator);
    });

    // Verify that inputs are sorted

    Bit local_order(1);
    inputs.for_each_pair([&](std::size_t i, auto& first, auto& second) {
        if (i < input_size_per_party - 1) {
            Bit lte = first.patient_id_concat_timestamp <= second.patient_id_concat_timestamp;
            local_order = local_order & lte;
        } else if (i >= input_size_per_party) {
            Bit gte = first.patient_id_concat_timestamp >= second.patient_id_concat_timestamp;
            local_order = local_order & gte;
        }
    });
    std::optional<Bit> order = utils.reduce_aggregates<Bit>(0, local_order, [](Bit& first, Bit& second) -> Bit {
        return first & second;
    });
    if (index == 0) {
        order.value().mark_output();
    }

    // Sort inputs and switch to blocked layout
    mage::dsl::parallel_bitonic_sorter(inputs);

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
    if (index == 0) {
        total.value().mark_output();
    }
}

void test_sorter(mage::WorkerID index, mage::WorkerID num_workers) {
    ShardedArray<Integer<8>> arr(128, index, num_workers, Layout::Cyclic);
    arr.for_each([](std::size_t i, auto& elem) {
        elem = Integer<8>(i < 64 ? i : 127 - i);
    });

    mage::dsl::parallel_bitonic_sorter(arr);

    arr.for_each([](std::size_t i, auto& elem) {
        elem.mark_output();
    });
}

int main(int argc, char** argv) {
    int input_size_per_party;
    WorkerID index = 0;
    WorkerID num_workers = 1;
    std::string problem_name = "aspirin_";
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " [config.yaml garbler/evaluator index] input_size_per_party" << std::endl;
        return 1;
    }

    mage::util::Configuration c(argv[1]);

    if (std::strcmp(argv[2], "garbler") != 0 && std::strcmp(argv[2], "evaluator") != 0) {
        std::cerr << "Second argument (" << argv[2] << ") is neither garbler not evaluator" << std::endl;
        return 1;
    }

    errno = 0;
    index = std::strtoull(argv[3], nullptr, 10);
    if (errno != 0) {
        std::perror("Third argument (index)");
        return 1;
    }

    const mage::util::ConfigValue& w = c[argv[2]]["workers"][index];

    errno = 0;
    input_size_per_party = std::strtoull(argv[4], nullptr, 10);
    if (errno != 0 || input_size_per_party == 0) {
        std::cerr << "Bad fourth argument (input size)" << std::endl;
        return 1;
    }
    problem_name.append(std::to_string(input_size_per_party));
    problem_name.append("_");
    problem_name.append(std::to_string(index));

    mage::memprog::DefaultPipeline planner(problem_name, w);
    planner.set_verbose(true);
    planner.plan(&p, [=]() {
        create_parallel_aspirin_circuit(index, num_workers, input_size_per_party);
    });

    std::cout << std::endl;

    const mage::memprog::DefaultPipelineStats& stats = planner.get_stats();

    std::cout << "Phase Times (ms): " << stats.placement_duration.count() << " "
        << stats.replacement_duration.count() << " " << stats.scheduling_duration.count() << std::endl;

    return 0;
}
