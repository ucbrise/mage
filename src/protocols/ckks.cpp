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
#include <cstdint>
#include "engine/addmultiply.hpp"
#include "protocols/registry.hpp"
#include "protocols/ckks.hpp"
#include "protocols/ckks_constants.hpp"

namespace mage::protocols::ckks {
    void run_ckks(const EngineOptions& args) {
        std::string file_base = args.problem_name + "_" + std::to_string(args.self_id);
        std::string prog_file = file_base + ".memprog";
        std::string output_file = file_base + ".output";
        std::string input_file = file_base + "_garbler.input";

        std::chrono::time_point<std::chrono::steady_clock> start;
        std::chrono::time_point<std::chrono::steady_clock> end;

        util::Configuration& c = *args.config;
        {
            CKKSEngine p(input_file.c_str(), output_file.c_str());
            start = std::chrono::steady_clock::now();
            engine::AddMultiplyEngine executor(args.cluster, c["parties"][args.party_id]["workers"][args.self_id], p, prog_file.c_str());
            executor.execute_program();
            end = std::chrono::steady_clock::now();
        }
        std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << ms.count() << " ms" << std::endl;
    }

    RegisterProtocol ckks("ckks", "Homomorphic Encryption for Arithmetic of Approximate Numbers", run_ckks, "ckks_plugin");

    memprog::AllocationSize ckks_physical_size(std::uint64_t logical_width, memprog::PlaceableType type) {
        std:uint64_t result = UINT64_MAX;
        switch (type) {
        case memprog::PlaceableType::Ciphertext:
            result = ckks_ciphertext_size(logical_width, true);
            break;
        case memprog::PlaceableType::Plaintext:
            result = ckks_plaintext_size(logical_width);
            break;
        case memprog::PlaceableType::DenormalizedCiphertext:
            result = ckks_ciphertext_size(logical_width, false);
            break;
        }
        if (result == UINT64_MAX) {
            throw memprog::InvalidPlacementException("ckks", logical_width, type);
        }
        return result;
    }

    RegisterPlacementPlugin ckks_plugin("ckks_plugin", "Object's MAGE-virtual size is the size of a CKKS ciphertext/plaintext in bytes", ckks_physical_size);
}
