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
#include "dsl/graph.hpp"
#include "dsl/sort.hpp"
#include <iostream>
#include <string>

using namespace mage::dsl;

template <BitWidth bits, Graph* g>
struct Input {
    Integer<bits, g> patient_id_concat_timestamp;
    Bit<g> diagnosis; // or aspirin prescription

    static void comparator(Input<bits, g>& arg0, Input<bits, g>& arg1) {
        Bit<g> predicate = arg0.patient_id_concat_timestamp > arg1.patient_id_concat_timestamp;
        Integer<bits, g>::swap_if(predicate, arg0.patient_id_concat_timestamp, arg1.patient_id_concat_timestamp);
        Bit<g>::swap_if(predicate, arg0.diagnosis, arg1.diagnosis);
    }
};

template <Graph* g, BitWidth patient_id_bits = 32, BitWidth timestamp_bits = 32, BitWidth result_bits = 32>
void create_aspirin_circuit(int input_size_per_party) {
    int input_array_length = input_size_per_party * 2;
    auto* inputs = new Input<patient_id_bits + timestamp_bits, g>[input_array_length];

    for (int i = 0; i != input_array_length; i++) {
        inputs[i].patient_id_concat_timestamp.mark_input();
        inputs[i].diagnosis.mark_input();
    }

    // Verify the input first.
    Bit<g> order(1);
    for (int i = 0; i < input_size_per_party - 1; i++) {
        Bit<g> lte = inputs[i].patient_id_concat_timestamp <= inputs[i+1].patient_id_concat_timestamp;
        order = order & lte;
    }
    for (int i = input_size_per_party; i < 2 * input_size_per_party - 1; i++) {
        Bit<g> gte = inputs[i].patient_id_concat_timestamp >= inputs[i+1].patient_id_concat_timestamp;
        order = order & gte;
    }
	order.mark_output();

    // Merge the two arrays, sorted ascending by patient_id_concat_timestamp
    bitonic_sorter(inputs, input_array_length);

    // Now, for each input, check if it and the next input have the same patient, but the first is a diagnosis and the second isn't.
    Integer<result_bits, g> total(0);
    for (int i = 0; i < input_array_length; i++) {
        Bit<g> add = inputs[i].diagnosis & ~inputs[i+1].diagnosis;
        Integer<patient_id_bits, g> patient_id_i = inputs[i].patient_id_concat_timestamp.template slice<patient_id_bits>(timestamp_bits);
        Integer<patient_id_bits, g> patient_id_ip1 = inputs[i+1].patient_id_concat_timestamp.template slice<patient_id_bits>(timestamp_bits);
        add = add & (patient_id_i == patient_id_ip1);
        Integer next = total.increment();
        total = Integer<result_bits, g>::select(add, total, next);
    }

    total.mark_output();

    delete[] inputs;
}

Graph graph;

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
	filename.append(".txt");

	create_aspirin_circuit<&graph>(input_size_per_party);

    std::cout << "Created graph with " << graph.num_vertices() << " vertices" << std::endl;
    return 0;
}
