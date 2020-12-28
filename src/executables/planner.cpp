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
#include "protocols/registry.hpp"
#include "util/config.hpp"

using mage::programs::ProgramOptions;
using mage::programs::RegisteredProgram;
using mage::protocols::RegisteredProtocol;
using mage::protocols::RegisteredPlacementPlugin;
using mage::util::Registry;

int main(int argc, char** argv) {
    if (argc != 7) {
        std::cerr << "Usage: " << argv[0] << " program_name protocol/plugin config.yaml party_id worker_index input_size" << std::endl;
        Registry<RegisteredProgram>::print_all("programs", std::cerr);
        return EXIT_FAILURE;
    }

    std::string program_name(argv[1]);
    const RegisteredProgram* prog = Registry<RegisteredProgram>::look_up_by_name(program_name);
    if (prog == nullptr) {
        std::cerr << program_name << " is not a valid program name. "; // lack of std::endl is intentional
        Registry<RegisteredProgram>::print_all("programs", std::cerr);
        return EXIT_FAILURE;
    }

    std::string protocol(argv[2]);
    std::string plugin_name;
    mage::memprog::PlacementPlugin plugin;
    const RegisteredProtocol* prot = Registry<RegisteredProtocol>::look_up_by_name(protocol);
    if (prot == nullptr) {
        const RegisteredPlacementPlugin* plug = Registry<RegisteredPlacementPlugin>::look_up_by_name(protocol);
        if (plug == nullptr) {
            std::cerr << protocol << " is not a valid protocol name or plugin name. "; // lack of std::endl is intentional
            Registry<RegisteredProtocol>::print_all("protocols", std::cerr);
            Registry<RegisteredPlacementPlugin>::print_all("plugins", std::cerr);
            return EXIT_FAILURE;
        }
        plugin = plug->get_placement_plugin();
    } else {
        plugin = prot->get_placement_plugin();
    }

    mage::util::Configuration c(argv[3]);

    std::optional<std::uint32_t> party_id = mage::protocols::parse_party_id(argv[4]);
    if (!party_id.has_value()) {
        std::cerr << "Invalid party_id (try \"garbler\", \"evaluator\", or an integer)" << std::endl;
        return EXIT_FAILURE;
    }

    mage::WorkerID num_workers = c["parties"][*party_id]["workers"].get_size();

    errno = 0;
    mage::WorkerID index = std::strtoull(argv[5], nullptr, 10);
    if (errno != 0) {
        std::perror("Fourth argument (index)");
        return EXIT_FAILURE;
    }
    if (index >= num_workers) {
        std::cerr << "Worker index is " << index << " but there are only " << num_workers << " workers" << std::endl;
        return EXIT_FAILURE;
    }

    errno = 0;
    std::uint64_t problem_size = std::strtoull(argv[6], nullptr, 10);
    if (errno != 0 || problem_size == 0) {
        std::cerr << "Bad fifth argument (input size)" << std::endl;
        return 1;
    }

    const mage::util::ConfigValue& w = c["parties"][*party_id]["workers"][index];

    ProgramOptions args = {};
    args.worker_config = &w;
    args.num_workers = num_workers;
    args.worker_index = index;
    args.problem_size = problem_size;

    std::string problem_name = program_name + "_" + std::to_string(problem_size) + "_" + std::to_string(index);

    mage::memprog::DefaultPipeline planner(problem_name, w);
    planner.set_verbose(true);
    planner.plan(&mage::programs::program_ptr, prot->get_placement_plugin(), [prog, &args]() {
        (*prog)(args);
    });

    std::cout << std::endl;

    const mage::memprog::DefaultPipelineStats& stats = planner.get_stats();

    std::cout << "Phase Times (ms): " << stats.placement_duration.count() << " "
        << stats.replacement_duration.count() << " " << stats.scheduling_duration.count() << std::endl;

    return EXIT_SUCCESS;
}
