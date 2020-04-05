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

#include <cerrno>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include "circuit.hpp"
#include "gate.hpp"
#include "planner/emp.hpp"
#include "platform/filesystem.hpp"
#include "platform/memory.hpp"

using namespace mage;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <EMP Circuit File>" << std::endl;
        return 1;
    }

    std::ifstream emp_file(argv[1]);
    if (!emp_file.is_open()) {
        std::cerr << "Could not open " << argv[1] << ": " << strerror(errno) << std::endl;
        return 1;
    }

    CircuitInfo header;
    if (!planner::parse_emp_circuit_header(emp_file, header)) {
        std::cerr << "Could not parse circuit header in " << argv[1] << std::endl;
        return 1;
    }

    std::filesystem::path emp_path(argv[1]);
    std::string out_file(emp_path.filename());
    out_file.append(".cir");
    std::size_t out_file_length = sizeof(Circuit) + header.num_gates * sizeof(CircuitGate);

    platform::MappedFile<Circuit> mapped(out_file.c_str(), out_file_length);

    Circuit* circuit = mapped.mapping();
    circuit->header = header;
    std::uint64_t num_parsed = planner::parse_emp_circuit_gates(emp_file, &circuit->gates[0], header);
    if (num_parsed != header.num_gates) {
        std::cerr << "Could not parse gates in " << argv[1] << ": failed on gate " << (num_parsed + 1) << "/" << header.num_gates << std::endl;
        return 1;
    }

    return 0;
}
