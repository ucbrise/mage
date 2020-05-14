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
#include <iostream>
#include <string>
#include "engine/engine.hpp"
#include "engine/singlecore.hpp"
#include "platform/network.hpp"
#include "schemes/halfgates.hpp"
#include "schemes/plaintext.hpp"

using namespace mage;

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " garble/evaluate program_name host:port" << std::endl;
        return 1;
    }

    /* Decide if we're garbling or evaluating. */

    bool plaintext = false;
    bool garble = false;
    if (std::strcmp(argv[1], "garble") == 0) {
        garble = true;
    } else if (std::strcmp(argv[1], "evaluate") == 0) {
        garble = false;
    } else if (std::strcmp(argv[1], "plaintext") == 0) {
        plaintext = true;
    } else {
        std::cerr << "First argument must be \"garble\" or \"evaluate\"" << std::endl;
        return 1;
    }

    /* Generate the file names. */

    std::string file_base(argv[2]);

    std::string prog_file(file_base);
    prog_file.append(".memprog");

    std::string input_file(file_base);
    input_file.append(".input");

    std::string output_file(file_base);
    output_file.append(".output");

    if (plaintext) {
        schemes::PlaintextEvaluator p(input_file.c_str(), output_file.c_str());
        engine::SingleCoreEngine executor(prog_file.c_str(), "swapfile", p);
        executor.execute_program();
        return 0;
    }

    /* Parse the host/port. */

    std::string host(argv[3]);
    std::string port;
    std::size_t colon_index = host.find_last_of(':');
    if (colon_index == std::string::npos) {
        port = host;
        host = "127.0.0.1";
    } else {
        port = host.substr(colon_index + 1);
        host.erase(colon_index);
    }

    /* Create the network connection. */

    int socket;
    if (garble) {
        socket = platform::network_connect(host.c_str(), port.c_str());

        schemes::HalfGatesGarbler p(input_file.c_str(), output_file.c_str(), socket);
        engine::SingleCoreEngine executor(prog_file.c_str(), "garbler_swapfile", p);
        executor.execute_program();
    } else {
        socket = platform::network_accept(port.c_str());

        schemes::HalfGatesEvaluator p(input_file.c_str(), socket);
        engine::SingleCoreEngine executor(prog_file.c_str(), "evaluator_swapfile", p);
        executor.execute_program();
    }

    platform::network_close(socket);
}
