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
#include "memprog/annotation.hpp"
#include "memprog/program.hpp"
#include "memprog/replacement.hpp"
#include "memprog/scheduling.hpp"
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

std::uint8_t page_shift = 12; // 64 KiB
std::uint64_t num_pages = 1 << 10;
std::uint64_t max_in_flight = 256;
// std::uint64_t num_pages = 65536 * 3;

// About 27 GiB
// std::uint64_t num_pages = 1769472;

int main(int argc, char** argv) {
    int input_size_per_party;
    std::string filename = "aspirin_";
    unsigned long long index = 0;
    if (argc == 5) {
        mage::util::Configuration c(argv[1]);

        if (std::strcmp(argv[2], "garbler") != 0 && std::strcmp(argv[2], "evaluator") != 0) {
            std::cerr << "Second argument (" << argv[2] << ") is neither garbler not evaluator" << std::endl;
            return 1;
        }
        const mage::util::ConfigValue& p = c[argv[2]];

        errno = 0;
        index = std::strtoull(argv[3], nullptr, 10);
        if (errno != 0) {
            std::perror("Third argument (index)");
            return 1;
        }

        const mage::util::ConfigValue& w = p["workers"][index];
        page_shift = w["page_shift"].as_int();
        max_in_flight = w["max_in_flight_swaps"].as_int();
        num_pages = w["num_available_pages"].as_int();

        errno = 0;
        input_size_per_party = std::strtoull(argv[4], nullptr, 10);
        if (errno != 0 || input_size_per_party == 0) {
            std::cerr << "Bad fourth argument (input size)" << std::endl;
            return 1;
        }

        errno = 0;
        input_size_per_party = std::strtoull(argv[4], nullptr, 10);
        if (errno != 0 || input_size_per_party == 0) {
            std::cerr << "Bad fourth argument (input size)" << std::endl;
            return 1;
        }
    } else if (argc == 2) {
        errno = 0;
        input_size_per_party = std::strtoull(argv[1], nullptr, 10);
        if (errno != 0 || input_size_per_party == 0) {
            std::cerr << "Bad second argument (input size)" << std::endl;
            return 1;
        }
    } else {
        std::cerr << "Usage: " << argv[0] << " [config.yaml garbler/evaluator index] input_size_per_party" << std::endl;
        return 1;
    }
    filename.append(std::to_string(input_size_per_party));
    filename.append("_");
    filename.append(std::to_string(index));

    std::string program_filename = filename;
    program_filename.append(".prog");

    auto program_start = std::chrono::steady_clock::now();
    {
        mage::memprog::DefaultProgram program(program_filename, page_shift);
        p = &program;
        create_aspirin_circuit(input_size_per_party);
        p = nullptr;
        std::cout << "Created program with " << program.num_instructions() << " instructions" << std::endl;
    }
    auto program_end = std::chrono::steady_clock::now();

    auto replacement_start = std::chrono::steady_clock::now();
    std::string ann_filename = filename;
    ann_filename.append(".ann");
    mage::memprog::annotate_program(ann_filename, program_filename, page_shift);
    std::cout << "Computed annotations" << std::endl;

    std::string repprog_filename = filename;
    repprog_filename.append(".repprog");
    {
        mage::memprog::BeladyAllocator allocator(repprog_filename, program_filename, ann_filename, num_pages, page_shift);
        allocator.allocate();
        std::cout << "Finished replacement stage: " << allocator.get_num_swapouts() << " swapouts, " << allocator.get_num_swapins() << " swapins" << std::endl;
    }
    auto replacement_end = std::chrono::steady_clock::now();

    auto scheduling_start = std::chrono::steady_clock::now();

    // {
    //     std::string memprog_filename = filename;
    //     memprog_filename.append(".memprog");
    //     mage::memprog::NOPScheduler scheduler(repprog_filename, memprog_filename);
    //     scheduler.schedule();
    //     std::cout << "Finished scheduling swaps" << std::endl;
    // }

    {
        std::string memprog_filename = filename;
        memprog_filename.append(".memprog");
        mage::memprog::BackdatingScheduler scheduler(repprog_filename, memprog_filename, 10000, max_in_flight);
        scheduler.schedule();
        std::cout << "Finished scheduling swaps: " << scheduler.get_num_allocation_failures() << " allocation failures, " << scheduler.get_num_synchronous_swapins() << " synchronous swapins" << std::endl;
    }

    auto scheduling_end = std::chrono::steady_clock::now();

    std::cout << std::endl;

    std::cout << "Phase Times (ms): ";
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(program_end - program_start).count() << " ";
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(replacement_end - replacement_start).count() << " ";
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(scheduling_end - scheduling_start).count();
    std::cout << std::endl;

    return 0;
}
