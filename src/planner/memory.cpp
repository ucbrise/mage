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

#include "planner/memory.hpp"

#include <cassert>
#include <cstdint>

#include <algorithm>
#include <iostream>
#include <vector>

namespace mage::planner {
    std::uint64_t compute_max_working_set_size(Circuit& c) {
        std::uint64_t max_working_set_size = 0;
        assert(c.header.magic == circuit_magic);

        /* At the very end, the working set consists of all wires. */
        std::uint64_t working_set_size = c.header.num_outputs;
        std::vector<bool> working_set(c.get_num_wires(), false);
        for (std::uint64_t wire = c.get_num_wires() - c.header.num_outputs; wire != c.get_num_wires(); wire++) {
            working_set[wire] = true;
        }

        /* Now, work backwards. */
        for (GateID i = c.header.num_gates - 1; i != UINT64_MAX; i--) {
            CircuitGate* gate = &c.gates[i];
            WireID output_wire = c.gate_to_output_wire(i);
            if (working_set[output_wire]) {
                /* Otherwise, just skip this gate. */
                if (!working_set[gate->input1_wire]) {
                    working_set[gate->input1_wire] = true;
                    working_set_size++;
                }
                if (!working_set[gate->input2_wire]) {
                    working_set[gate->input2_wire] = true;
                    working_set_size++;
                }
                max_working_set_size = std::max(max_working_set_size, working_set_size);
                working_set[output_wire] = false;
                working_set_size--;
            } else {
                gate->dead = true;
            }
        }

        assert(working_set_size == c.get_num_input_wires());
        for (std::uint64_t i = 0; i != c.get_num_input_wires(); i++) {
            assert(working_set[i]);
        }

        return max_working_set_size;
    }
}
