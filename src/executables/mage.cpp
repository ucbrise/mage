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
#include <chrono>
#include <iostream>
#include <string>
#include "engine/engine.hpp"
#include "engine/singlecore.hpp"
#include "engine/halfgates.hpp"
#include "engine/plaintext.hpp"
#include "engine/tfhe.hpp"
#include "platform/network.hpp"
#include "util/config.hpp"
#include "util/resourceset.hpp"

using namespace mage;

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " config.yaml garble/evaluate/tfhe worker_id program_name" << std::endl;
        return 1;
    }

    /* Parse the config.yaml file. */

    util::Configuration c(argv[1]);

    /* Parse the worker ID. */

    WorkerID self_id;
    std::istringstream self_id_stream(argv[3]);
    self_id_stream >> self_id;

    /* Validate the config.yaml file for running the computation. */

    if (c.get("garbler") == nullptr) {
        std::cerr << "Garbler not present in configuration file" << std::endl;
        return 1;
    }
    if (c.get("evaluator") == nullptr) {
        std::cerr << "Evaluator not present in configuration file" << std::endl;
        return 1;
    }
    if (c["garbler"]["workers"].get_size() != c["evaluator"]["workers"].get_size()) {
        std::cerr << "Garbler has " << c["garbler"]["workers"].get_size() << " workers but evaluator has " << c["evaluator"]["workers"].get_size() << " workers --- must be equal" << std::endl;
        return 1;
    }
    if (self_id >= c["garbler"]["workers"].get_size()) {
        std::cerr << "Worker index is " << self_id << " but only " << c["garbler"]["workers"].get_size() << " workers are specified" << std::endl;
        return 1;
    }

    /* Decide if we're garbling or evaluating. */

    bool plaintext = false;
    bool garble = false;
    bool tfhe = false;
    const util::ConfigValue* party;
    const util::ConfigValue* other_party;
    if (std::strcmp(argv[2], "garble") == 0) {
        garble = true;
        party = &c["garbler"];
        other_party = &c["evaluator"];
    } else if (std::strcmp(argv[2], "evaluate") == 0) {
        garble = false;
        party = &c["evaluator"];
        other_party = &c["garbler"];
    } else if (std::strcmp(argv[2], "plaintext") == 0) {
        plaintext = true;
        party = &c["garbler"];
        other_party = &c["evaluator"];
    } else if (std::strcmp(argv[2], "tfhe") == 0) {
        tfhe = true;
        party = &c["garbler"];
        other_party = &c["evaluator"];
    } else {
        std::cerr << "Third argument must be \"garble\" or \"evaluate\"" << std::endl;
        return 1;
    }

    /* Generate the file names. */

    std::string file_base(argv[4]);
    file_base.append("_");
    file_base.append(std::to_string(self_id));

    std::string prog_file(file_base);
    prog_file.append(".memprog");

    std::string garbler_input_file(file_base);
    garbler_input_file.append("_garbler.input");

    std::string evaluator_input_file(file_base);
    evaluator_input_file.append("_evaluator.input");

    std::string output_file(file_base);
    output_file.append(".output");

    auto cluster = std::make_shared<mage::engine::ClusterNetwork>(self_id);
    std::string err = cluster->establish(*party);
    if (!err.empty()) {
        std::cerr << err << std::endl;
        std::abort();
    }

    std::chrono::time_point<std::chrono::steady_clock> start;
    std::chrono::time_point<std::chrono::steady_clock> end;

    if (plaintext) {
        engine::PlaintextEvaluationEngine p(garbler_input_file.c_str(), evaluator_input_file.c_str(), output_file.c_str());
        start = std::chrono::steady_clock::now();
        engine::SingleCoreEngine executor(cluster, (*party)["workers"][self_id], p, prog_file.c_str());
        executor.execute_program();
        end = std::chrono::steady_clock::now();
        std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cerr << ms.count() << " ms" << std::endl;
        return 0;
    }

    /* Perform the computation. */

    if (tfhe) {
        engine::TFHEEngine p(garbler_input_file.c_str(), evaluator_input_file.c_str(), output_file.c_str());
        start = std::chrono::steady_clock::now();
        engine::SingleCoreEngine executor(cluster, (*party)["workers"][self_id], p, prog_file.c_str());
        executor.execute_program();
        end = std::chrono::steady_clock::now();
        std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cerr << ms.count() << " ms" << std::endl;
        return 0;
    }

    if (garble) {
        const util::ConfigValue& opposite_worker = (*other_party)["workers"][self_id];
        if (opposite_worker.get("external_host") == nullptr || opposite_worker.get("external_port") == nullptr) {
            std::cerr << "Opposite party's external network information is not specified" << std::endl;
            return 1;
        }

        engine::HalfGatesGarblingEngine p(cluster, garbler_input_file.c_str(), output_file.c_str(), opposite_worker["external_host"].as_string().c_str(), opposite_worker["external_port"].as_string().c_str());
        engine::SingleCoreEngine executor(cluster, (*party)["workers"][self_id], p, prog_file.c_str());
        start = std::chrono::steady_clock::now();
        executor.execute_program();
    } else {
        const util::ConfigValue& worker = (*party)["workers"][self_id];
        if (worker.get("external_host") == nullptr || worker.get("external_port") == nullptr) {
            std::cerr << "This party's external network information is not specified" << std::endl;
            return 1;
        }

        engine::HalfGatesEvaluationEngine p(evaluator_input_file.c_str(), worker["external_port"].as_string().c_str());
        engine::SingleCoreEngine executor(cluster, (*party)["workers"][self_id], p, prog_file.c_str());
        start = std::chrono::steady_clock::now();
        executor.execute_program();
    }
    end = std::chrono::steady_clock::now();

    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << ms.count() << " ms" << std::endl;
}
