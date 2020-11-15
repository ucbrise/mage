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
#include <iostream>
#include <string>
#include "addr.hpp"
#include "memprog/pipeline.hpp"
#include "programs/registry.hpp"
#include "util/config.hpp"

using mage::programs::ProgramOptions;
using mage::programs::RegisteredProgram;

static void print_valid_program_names() {
    if (RegisteredProgram::get_registry().size() == 0) {
        std::cerr << "There are no valid program names in this build." << std::endl;
    } else {
        std::cerr << "Valid program names:" << std::endl;
        for (const auto& [name, prog] : RegisteredProgram::get_registry()) {
            std::cerr << name << " - " << prog.get_description() << std::endl;
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "Usage: " << argv[0] << " program_name config.yaml garbler/evaluator index input_size" << std::endl;
        print_valid_program_names();
        return EXIT_FAILURE;
    }

    std::string program_name(argv[1]);
    const RegisteredProgram* prog = RegisteredProgram::look_up_by_name(program_name);
    if (prog == nullptr) {
        std::cerr << program_name << " is not a valid program name. "; // lack of std::endl is intentional
        print_valid_program_names();
        return EXIT_FAILURE;
    }

    mage::util::Configuration c(argv[2]);

    if (std::strcmp(argv[3], "garbler") != 0 && std::strcmp(argv[3], "evaluator") != 0) {
        std::cerr << "Third argument (" << argv[3] << ") is neither garbler not evaluator" << std::endl;
        return EXIT_FAILURE;
    }
    mage::WorkerID num_workers = c[argv[3]]["workers"].get_size();

    errno = 0;
    mage::WorkerID index = std::strtoull(argv[4], nullptr, 10);
    if (errno != 0) {
        std::perror("Fourth argument (index)");
        return EXIT_FAILURE;
    }
    if (index >= num_workers) {
        std::cerr << "Worker index is " << index << " but there are only " << num_workers << " workers" << std::endl;
        return EXIT_FAILURE;
    }

    errno = 0;
    std::uint64_t problem_size = std::strtoull(argv[5], nullptr, 10);
    if (errno != 0 || problem_size == 0) {
        std::cerr << "Bad fifth argument (input size)" << std::endl;
        return 1;
    }

    const mage::util::ConfigValue& w = c[argv[3]]["workers"][index];

    ProgramOptions args = {};
    args.worker_config = &w;
    args.num_workers = num_workers;
    args.worker_index = index;
    args.problem_size = problem_size;

    std::string problem_name = program_name + "_" + std::to_string(problem_size) + "_" + std::to_string(index);

    mage::memprog::DefaultPipeline planner(problem_name, w);
    planner.set_verbose(true);
    planner.plan(&RegisteredProgram::program_ptr, [prog, &args]() {
        (*prog)(args);
    });

    std::cout << std::endl;

    const mage::memprog::DefaultPipelineStats& stats = planner.get_stats();

    std::cout << "Phase Times (ms): " << stats.placement_duration.count() << " "
        << stats.replacement_duration.count() << " " << stats.scheduling_duration.count() << std::endl;

    return EXIT_SUCCESS;
}
