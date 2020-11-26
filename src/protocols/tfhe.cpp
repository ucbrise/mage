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

#include <chrono>
#include "engine/andxor.hpp"
#include "protocols/registry.hpp"
#include "protocols/tfhe.hpp"

namespace mage::protocols::tfhe {
    void run_tfhe(const EngineOptions& args) {
        std::string file_base = args.problem_name + "_" + std::to_string(args.self_id);
        std::string prog_file = file_base + ".memprog";
        std::string output_file = file_base + ".output";
        std::string evaluator_input_file = file_base + "_evaluator.input";
        std::string garbler_input_file = file_base + "_garbler.input";

        std::chrono::time_point<std::chrono::steady_clock> start;
        std::chrono::time_point<std::chrono::steady_clock> end;

        util::Configuration& c = *args.config;
        TFHEEngine p(garbler_input_file.c_str(), evaluator_input_file.c_str(), output_file.c_str());
        start = std::chrono::steady_clock::now();
        engine::ANDXOREngine executor(args.cluster, c["garbler"]["workers"][args.self_id], p, prog_file.c_str());
        executor.execute_program();
        end = std::chrono::steady_clock::now();
        std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cerr << ms.count() << " ms" << std::endl;
    }

    RegisterProtocol tfhe("tfhe", "Fast Fully Homomorphic Encryption over the Torus", run_tfhe);
}
