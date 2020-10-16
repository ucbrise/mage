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
#include "dsl/integer.hpp"
#include "dsl/sort.hpp"
#include "memprog/pipeline.hpp"
#include "programfile.hpp"
#include "util/config.hpp"

using mage::BitWidth;
using mage::dsl::Party;

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
};

template <BitWidth patient_id_bits = 32, BitWidth timestamp_bits = 32, BitWidth result_bits = 32>
void create_aspirin_circuit(int input_size_per_party) {
    int input_array_length = input_size_per_party * 2;
    std::vector<Input<patient_id_bits + timestamp_bits>> inputs;

    for (int i = 0; i != input_array_length; i++) {
        inputs.emplace_back();
        inputs[i].patient_id_concat_timestamp.mark_input(i < input_size_per_party ? Party::Garbler : Party::Evaluator);
        inputs[i].diagnosis.mark_input(i < input_size_per_party ? Party::Garbler : Party::Evaluator);
    }

    // Verify the input first.
    Bit order(1);
    for (int i = 0; i < input_size_per_party - 1; i++) {
        Bit lte = inputs[i].patient_id_concat_timestamp <= inputs[i+1].patient_id_concat_timestamp;
        order = order & lte;
    }
    for (int i = input_size_per_party; i < 2 * input_size_per_party - 1; i++) {
        Bit gte = inputs[i].patient_id_concat_timestamp >= inputs[i+1].patient_id_concat_timestamp;
        order = order & gte;
    }
	order.mark_output();

    // Merge the two arrays, sorted ascending by patient_id_concat_timestamp
    mage::dsl::bitonic_sorter(inputs.data(), input_array_length);

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

int main(int argc, char** argv) {
    int input_size_per_party;
    mage::WorkerID index = 0;
    mage::WorkerID num_workers = 1;
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
        create_aspirin_circuit(input_size_per_party);
    });

    std::cout << std::endl;

    const mage::memprog::DefaultPipelineStats& stats = planner.get_stats();

    std::cout << "Phase Times (ms): " << stats.placement_duration.count() << " "
        << stats.replacement_duration.count() << " " << stats.scheduling_duration.count() << std::endl;

    return 0;
}
