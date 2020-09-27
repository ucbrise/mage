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

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "util/binaryfile.hpp"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " input_size_per_party num_workers" << std::endl;
        return 0;
    }
    int input_size = std::stoi(std::string(argv[1]));
    int num_workers = std::stoi(std::string(argv[2]));

    std::vector<std::unique_ptr<mage::util::BinaryFileWriter>> garbler_writers(num_workers);
    std::vector<std::unique_ptr<mage::util::BinaryFileWriter>> evaluator_writers(num_workers);

    for (int i = 0; i != num_workers; i++) {
        std::string garbler_file("aspirin_");
        garbler_file.append(std::to_string(input_size));
        garbler_file.append("_");
        garbler_file.append(std::to_string(i));
        garbler_file.append("_garbler.input");
        garbler_writers[i] = std::make_unique<mage::util::BinaryFileWriter>(garbler_file.c_str());

        std::string evaluator_file("aspirin_");
        evaluator_file.append(std::to_string(input_size));
        evaluator_file.append("_");
        evaluator_file.append(std::to_string(i));
        evaluator_file.append("_evaluator.input");
        evaluator_writers[i] = std::make_unique<mage::util::BinaryFileWriter>(evaluator_file.c_str());
    }

    for (std::uint64_t i = 0; i != input_size * 2; i++) {
        std::uint64_t party = (i % num_workers);
        if (i < input_size) {
            garbler_writers[party]->write64((i << 32) | 1);
            garbler_writers[party]->write1(i == 0 ? 0 : 1);
            /* All patients except patient 0 have a diagnosis at t = 1. */
        } else {
            evaluator_writers[party]->write64(((2 * input_size - i - 1) << 32) | 2);
            evaluator_writers[party]->write1(0);
            /* All patients were prescribed aspirin at t = 2. */
        }
    }

    /* Correct output is 1, (input_size - 1). */

    return 0;
}
