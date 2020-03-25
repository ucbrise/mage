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

#include "planner/emp.hpp"

#include <istream>
#include <sstream>
#include <string>

#include "circuit.hpp"

namespace mage::planner {
    bool parse_emp_circuit_header(std::istream& emp_file, CircuitInfo& header) {
        {
            std::string line;
            std::getline(emp_file, line);
            if (!emp_file.good()) {
                return false;
            }
            std::istringstream stream(line);
            stream >> header.num_gates;
            stream >> header.num_wires;
            if (stream.fail() || stream.bad()) {
                return false;
            }
        }
        {
            std::string line;
            std::getline(emp_file, line);
            if (!emp_file.good()) {
                return false;
            }
            std::istringstream stream(line);
            stream >> header.num_party1_inputs;
            stream >> header.num_party2_inputs;
            stream >> header.num_outputs;
            if (stream.fail() || stream.bad()) {
                return false;
            }
        }
        return true;
    }

    std::uint64_t parse_emp_circuit_gates(std::istream& emp_file, std::uint64_t num_gates, CircuitGate* gates) {
        std::uint64_t gate_number = 0;
        while (gate_number != num_gates) {
            CircuitGate* gate = &gates[gate_number];
            std::string line;
            std::getline(emp_file, line);
            if (gate_number == num_gates - 1) {
                if (emp_file.fail() || emp_file.bad()) {
                    return gate_number;
                }
            } else if (!emp_file.good()) {
                return gate_number;
            }
            std::istringstream stream(line);
            int num_inputs;
            std::string type;
            stream >> num_inputs;
            if (stream.eof()) {
                continue;
            }

            gate_number++;
            if (num_inputs == 1) {
                // NOT gate
                stream >> num_inputs; // drop first item
                stream >> gate->input1_wire;
                gate->input2_wire = gate->input1_wire;
                stream >> gate->output_wire;
            } else if (num_inputs == 2) {
                // AND or XOR gate
                stream >> num_inputs; // drop first item
                stream >> gate->input1_wire;
                stream >> gate->input2_wire;
                stream >> gate->output_wire;
            } else {
                return gate_number;
            }
            stream >> type;
            if (stream.fail() || stream.bad()) {
                return gate_number;
            }
            if (type == "AND") {
                gate->type = GateType::AND;
            } else if (type == "XOR") {
                gate->type = GateType::XOR;
            } else if (type == "INV") {
                gate->type = GateType::NOT;
            } else {
                return gate_number;
            }
        }
        return num_gates;
    }
}
