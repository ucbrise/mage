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

#ifndef MAGE_CIRCUIT_HPP_
#define MAGE_CIRCUIT_HPP_

#include <cassert>
#include <cstdint>

#include "gate.hpp"

namespace mage {
    const constexpr std::uint64_t circuit_magic = UINT64_C(0xfd908b96364a2e73);
    using WireID = std::uint64_t;
    using GateID = std::uint64_t;

    struct CircuitInfo {
        std::uint64_t num_gates;
        std::uint64_t num_party1_inputs;
        std::uint64_t num_party2_inputs;
        std::uint64_t num_outputs;
        std::uint64_t magic;

        std::uint64_t get_num_input_wires() const {
            return this->num_party1_inputs + this->num_party2_inputs;
        }

        std::uint64_t get_num_wires() const {
            return this->num_gates + this->get_num_input_wires();
        }

        WireID gate_to_output_wire(GateID gate) const {
            assert(gate < this->num_gates);
            return gate + this->get_num_input_wires();
        }

        GateID output_wire_to_gate(WireID wire) const {
            assert(wire > this->get_num_input_wires());
            assert(wire < this->get_num_wires());
            return wire - this->get_num_input_wires();
        }
    };

    struct CircuitGate {
        WireID input1_wire;
        WireID input2_wire;
        GateType type;
        bool dead; /* Used for dead gate elimination. */
    };

    struct Circuit {
        CircuitInfo header;
        CircuitGate gates[0];

        std::uint64_t get_num_input_wires() const {
            return this->header.get_num_input_wires();
        }

        std::uint64_t get_num_wires() const {
            return this->header.get_num_wires();
        }

        WireID gate_to_output_wire(GateID gate) const {
            return this->header.gate_to_output_wire(gate);
        }

        GateID output_wire_to_gate(WireID wire) const {
            return this->header.output_wire_to_gate(wire);
        }
    };
}

#endif
