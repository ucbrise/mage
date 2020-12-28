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
#include "addr.hpp"
#include "protocols/registry.hpp"
#include "platform/network.hpp"
#include "util/config.hpp"

using namespace mage;
using mage::protocols::RegisteredProtocol;
using mage::protocols::EngineOptions;
using mage::util::Registry;

int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "Usage: " << argv[0] << " protocol config.yaml party_id worker_id program_name" << std::endl;
        return EXIT_FAILURE;
    }

    /* Parse the protocol name. */

    std::string protocol_name(argv[1]);
    const RegisteredProtocol* prot_ptr = Registry<RegisteredProtocol>::look_up_by_name(protocol_name);
    if (prot_ptr == nullptr) {
        std::cerr << protocol_name << " is not a valid protocol name. "; // lack of std::endl is intentional
        Registry<RegisteredProtocol>::print_all("protocols", std::cerr);
        return EXIT_FAILURE;
    }

    /* Parse the config.yaml file. */

    util::Configuration c(argv[2]);

    /* Parse the party ID. */

    std::optional<PartyID> party_id = mage::protocols::parse_party_id(argv[3]);
    if (!party_id.has_value()) {
        std::cerr << "Invalid party_id (try \"garbler\", \"evaluator\", or an integer)" << std::endl;
        return EXIT_FAILURE;
    }

    /* Parse the worker ID. */

    WorkerID self_id;
    std::istringstream self_id_stream(argv[4]);
    self_id_stream >> self_id;

    /* Establish cluster networking. */

    std::size_t buffer_size = 1 << 18;

    /* TODO: do this more systematically. */
    if (protocol_name == "ckks") {
        buffer_size = 1 << 20;
    }

    auto cluster = std::make_shared<mage::engine::ClusterNetwork>(self_id, buffer_size);
    std::string err = cluster->establish(c["parties"][*party_id]);
    if (!err.empty()) {
        std::cerr << err << std::endl;
        return EXIT_FAILURE;
    }

    /* Dispatch to the protocol. */

    EngineOptions args = {};
    args.config = &c;
    args.party_id = *party_id;
    args.self_id = self_id;
    args.cluster = cluster;
    args.problem_name = argv[5];

    const RegisteredProtocol& protocol = *prot_ptr;
    protocol(args);

    return EXIT_SUCCESS;
}
