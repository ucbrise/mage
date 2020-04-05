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

#include <iostream>
#include <filesystem>

#include "circuit.hpp"
#include "planner/graph.hpp"
#include "planner/memory.hpp"
#include "platform/memory.hpp"

using namespace mage;

int main(int argc, char** argv) {
    if (argc == 1) {
        std::cerr << "Usage: " << argv[0] << " <Circuit File>" << std::endl;
        return 1;
    }

    platform::MappedFile<Circuit> mapped(argv[1]);
    Circuit* circuit = mapped.mapping();

    std::cout << circuit->header.num_party1_inputs + circuit->header.num_party2_inputs << " inputs, " << circuit->header.num_outputs << " outputs" << std::endl;

    std::cout << "Computing working set of existing ordering... ";
    std::uint64_t max_working_set_size = planner::compute_max_working_set_size(*circuit);
    std::cout << max_working_set_size << " wires" << std::endl;

    std::cout << "Computing wire graph representation... ";
    planner::WireGraph wg(*circuit);
    std::cout << "done" << std::endl;

    std::filesystem::path circuit_path(argv[1]);
    std::string out_file(circuit_path.filename());
    out_file.append(".lin");

    std::cout << "Computing linearization... ";
    std::unique_ptr<planner::FileTraversalWriter> output(new planner::FileTraversalWriter(out_file));
    planner::FIFOKahnTraversal traversal(wg, std::move(output));
    traversal.traverse();
    std::cout << "done" << std::endl;

    return 0;
}
