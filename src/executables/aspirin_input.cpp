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

    std::string output_file("aspirin_");
    output_file.append(std::to_string(input_size));
    output_file.append(".input");

    mage::util::BinaryFileWriter writer(output_file.c_str());
    for (std::uint64_t i = 0; i != input_size * 2; i++) {
        if (i < input_size) {
            writer.write64((i << 32) | 1);
            writer.write1(i == 0 ? 0 : 1);
        } else {
            writer.write64(((2 * input_size - i - 1) << 32) | 2);
            writer.write1(0);
        }
    }

    /* Correct output is 1, (input_size - 1). */

    return 0;
}
