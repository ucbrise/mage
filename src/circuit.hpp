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

#include "gate.hpp"

namespace mage {
    struct CircuitInfo {
        std::uint64_t num_gates;
        std::uint64_t num_wires;
        std::uint64_t num_party1_inputs;
        std::uint64_t num_party2_inputs;
        std::uint64_t num_outputs;
    };

    struct CircuitGate {
        std::uint64_t input1_wire;
        std::uint64_t input2_wire;
        std::uint64_t output_wire;
        GateType type;
    };

    struct Circuit {
        CircuitInfo header;
        CircuitGate gates[0];
    };
}

#endif
