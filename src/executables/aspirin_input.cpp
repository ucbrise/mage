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
#include <string>
#include "util/binaryfile.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " input_size_per_party" << std::endl;
        return 0;
    }
    std::string input_size_per_party(argv[1]);
    int input_size = std::stoi(input_size_per_party);

    std::string garbler_file("aspirin_");
    garbler_file.append(std::to_string(input_size));
    garbler_file.append("_0_garbler.input");

    std::string evaluator_file("aspirin_");
    evaluator_file.append(std::to_string(input_size));
    evaluator_file.append("_0_evaluator.input");

    mage::util::BinaryFileWriter garbler_writer(garbler_file.c_str());
    mage::util::BinaryFileWriter evaluator_writer(evaluator_file.c_str());
    for (std::uint64_t i = 0; i != input_size * 2; i++) {
        if (i < input_size) {
            garbler_writer.write64((i << 32) | 1);
            garbler_writer.write1(i == 0 ? 0 : 1);
            /* All patients except patient 0 have a diagnosis at t = 1. */
        } else {
            evaluator_writer.write64(((2 * input_size - i - 1) << 32) | 2);
            evaluator_writer.write1(0);
            /* All patients were prescribed aspirin at t = 2. */
        }
    }

    /* Correct output is 1, (input_size - 1). */

    return 0;
}
