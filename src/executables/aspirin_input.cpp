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

    std::string output_file("aspirin_input_");
    output_file.append(argv[1]);

    mage::util::BinaryFileWriter writer(output_file);
    for (int i = 0; i != input_size * 2; i++) {
        writer.write64(0);
        writer.write1(0);
    }

    return 0;
}
