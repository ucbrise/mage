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

#include "dsl/integer.hpp"
#include "dsl/sort.hpp"
#include "memprog/annotation.hpp"
#include "memprog/replacement.hpp"
#include "programfile.hpp"
#include <iostream>
#include <string>

using namespace mage::dsl;
using mage::BitWidth;

template <BitWidth bits>
struct Input {
    Input(Program& p) : patient_id_concat_timestamp(p), diagnosis(p) {
    }

    Integer<bits> patient_id_concat_timestamp;
    Bit diagnosis; // or aspirin prescription

    static void comparator(Input<bits>& arg0, Input<bits>& arg1) {
        Bit predicate = arg0.patient_id_concat_timestamp > arg1.patient_id_concat_timestamp;
        Integer<bits>::swap_if(predicate, arg0.patient_id_concat_timestamp, arg1.patient_id_concat_timestamp);
        Bit::swap_if(predicate, arg0.diagnosis, arg1.diagnosis);
    }
};

template <BitWidth patient_id_bits = 32, BitWidth timestamp_bits = 32, BitWidth result_bits = 32>
void create_aspirin_circuit(Program& p, int input_size_per_party) {
    int input_array_length = input_size_per_party * 2;
    std::vector<Input<patient_id_bits + timestamp_bits>> inputs;
    inputs.resize(input_array_length, Input<patient_id_bits + timestamp_bits>(p));

    for (int i = 0; i != input_array_length; i++) {
        inputs[i].patient_id_concat_timestamp.mark_input();
        inputs[i].diagnosis.mark_input();
    }

    // Verify the input first.
    Bit order(1, p);
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
    bitonic_sorter(inputs.data(), input_array_length);

    // Now, for each input, check if it and the next input have the same patient, but the first is a diagnosis and the second isn't.
    Integer<result_bits> total(0, p);
    for (int i = 0; i < input_array_length - 1; i++) {
        Bit add = inputs[i].diagnosis & ~inputs[i+1].diagnosis;
        Integer<patient_id_bits> patient_id_i = inputs[i].patient_id_concat_timestamp.template slice<patient_id_bits>(timestamp_bits);
        Integer<patient_id_bits> patient_id_ip1 = inputs[i+1].patient_id_concat_timestamp.template slice<patient_id_bits>(timestamp_bits);
        add = add & (patient_id_i == patient_id_ip1);
        Integer next = total.increment();
        total = Integer<result_bits>::select(add, total, next);
    }

    total.mark_output();
}

std::uint8_t page_shift = 10;
std::uint64_t num_pages = 1 << 5;

// About 27 GiB
// std::uint64_t num_pages = 1769472;

int main(int argc, char** argv) {
    int input_size_per_party = 128;
    std::string filename = "aspirin_";
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " input_size_per_party" << std::endl;
        return 1;
    }
    if (argc == 2) {
        input_size_per_party = atoi(argv[1]);
    }
    filename.append(std::to_string(input_size_per_party));

    std::string program_filename = filename;
    program_filename.append(".prog");

    {
        mage::memprog::Program program(program_filename, page_shift);
        create_aspirin_circuit(program, input_size_per_party);
        std::cout << "Created program with " << program.num_instructions() << " instructions" << std::endl;
    }

    std::string ann_filename = filename;
    ann_filename.append(".ann");
    mage::memprog::annotate_program(ann_filename, program_filename, page_shift);
    std::cout << "Computed actual annotations" << std::endl;

    {
        std::string memprog_filename = filename;
        memprog_filename.append(".memprog");
        mage::memprog::BeladyAllocator allocator(memprog_filename, program_filename, ann_filename, num_pages, page_shift);
        allocator.allocate();
        std::cout << "Finished replacement stage: " << allocator.get_num_swapouts() << " swapouts, " << allocator.get_num_swapins() << " swapins" << std::endl;
    }

    return 0;
}
