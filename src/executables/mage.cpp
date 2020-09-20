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

#include <cstring>
#include <chrono>
#include <iostream>
#include <string>
#include "engine/engine.hpp"
#include "engine/singlecore.hpp"
#include "engine/halfgates.hpp"
#include "engine/plaintext.hpp"
#include "platform/network.hpp"
#include "util/resourceset.hpp"

using namespace mage;

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " config.yaml worker_id garble/evaluate program_name" << std::endl;
        return 1;
    }

    /* Parse the config.yaml file. */

    util::ResourceSet rs;
    std::string err = rs.from_yaml_file(argv[1]);
    if (!err.empty()) {
        std::cerr << err << std::endl;
        return 1;
    }

    /* Parse the worker ID. */

    WorkerID self_id;
    std::istringstream self_id_stream(argv[2]);
    self_id_stream >> self_id;

    /* Validate the config.yaml file for running the computation. */

    if (!rs.garbler.has_value()) {
        std::cerr << "Garbler not present in configuration file" << std::endl;
        return 1;
    }
    if (!rs.evaluator.has_value()) {
        std::cerr << "Evaluator not present in configuration file" << std::endl;
        return 1;
    }
    if (rs.garbler->workers.size() != rs.evaluator->workers.size()) {
        std::cerr << "Garbler has " << rs.garbler->workers.size() << " workers but evaluator has " << rs.evaluator->workers.size() << " workers --- must be equal" << std::endl;
        return 1;
    }
    if (self_id >= rs.garbler->workers.size()) {
        std::cerr << "Worker index is " << self_id << " but only " << rs.garbler->workers.size() << " workers are specified" << std::endl;
        return 1;
    }

    /* Decide if we're garbling or evaluating. */

    bool plaintext = false;
    bool garble = false;
    util::ResourceSet::Party* party;
    util::ResourceSet::Party* other_party;
    if (std::strcmp(argv[3], "garble") == 0) {
        garble = true;
        party = &(*rs.garbler);
        other_party = &(*rs.evaluator);
    } else if (std::strcmp(argv[3], "evaluate") == 0) {
        garble = false;
        party = &(*rs.evaluator);
        other_party = &(*rs.garbler);
    } else if (std::strcmp(argv[3], "plaintext") == 0) {
        plaintext = true;
        party = &(*rs.garbler);
        other_party = &(*rs.evaluator);
    } else {
        std::cerr << "Third argument must be \"garble\" or \"evaluate\"" << std::endl;
        return 1;
    }

    /* Generate the file names. */

    std::string file_base(argv[4]);

    std::string prog_file(file_base);
    prog_file.append(".memprog");

    std::string garbler_input_file(file_base);
    garbler_input_file.append("_garbler.input");

    std::string evaluator_input_file(file_base);
    evaluator_input_file.append("_evaluator.input");

    std::string output_file(file_base);
    output_file.append(".output");

    std::chrono::time_point<std::chrono::steady_clock> start;
    std::chrono::time_point<std::chrono::steady_clock> end;

    if (plaintext) {
        engine::PlaintextEvaluationEngine p(garbler_input_file.c_str(), evaluator_input_file.c_str(), output_file.c_str());
        start = std::chrono::steady_clock::now();
        engine::SingleCoreEngine executor(*party, self_id, p, prog_file.c_str());
        executor.execute_program();
        end = std::chrono::steady_clock::now();
        std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cerr << ms.count() << " ms" << std::endl;
        return 0;
    }

    /* Perform the computation. */
    if (garble) {
        util::ResourceSet::Worker& opposite_worker = other_party->workers[self_id];
        if (!opposite_worker.external_host.has_value() || !opposite_worker.external_port.has_value()) {
            std::cerr << "Opposite party's external network information is not specified" << std::endl;
            return 1;
        }

        engine::HalfGatesGarblingEngine p(garbler_input_file.c_str(), output_file.c_str(), opposite_worker.external_host->c_str(), opposite_worker.external_port->c_str());
        start = std::chrono::steady_clock::now();
        engine::SingleCoreEngine executor(*party, self_id, p, prog_file.c_str());
        executor.execute_program();
    } else {
        util::ResourceSet::Worker& worker = party->workers[self_id];
        if (!worker.external_host.has_value() || !worker.external_port.has_value()) {
            std::cerr << "This party's external network information is not specified" << std::endl;
            return 1;
        }

        engine::HalfGatesEvaluationEngine p(evaluator_input_file.c_str(), worker.external_port->c_str());
        start = std::chrono::steady_clock::now();
        engine::SingleCoreEngine executor(*party, self_id, p, prog_file.c_str());
        executor.execute_program();
    }
    end = std::chrono::steady_clock::now();

    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << ms.count() << " ms" << std::endl;
}
