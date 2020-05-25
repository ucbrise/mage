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

#define BOOST_TEST_DYN_LINK
#include "boost/test/unit_test.hpp"

#include <cstdint>
#include <iostream>

#include "circuit.hpp"
#include "planner/traversal.hpp"

using namespace mage;
using namespace mage::planner;
class TestMRUTraversal : public PriorityTraversal {
public:
    TestMRUTraversal(const WireGraph& wg, std::unique_ptr<TraversalWriter>&& out, const Circuit& c)
        : PriorityTraversal(wg, std::move(out), c) {
        for (WireID i = 0; i != wg.get_num_input_wires(); i++) {
            this->ready_gate_outputs.insert(this->compute_score(i), i);
        }
    }

    std::int64_t compute_score(WireID gate_output) override {
        if (gate_output < this->circuit.get_num_input_wires()) {
            return gate_output; // deterministic order for inputs
        }
        GateID gate_id = this->circuit.output_wire_to_gate(gate_output);
        WireID input1 = this->circuit.gates[gate_id].input1_wire;
        WireID input2 = this->circuit.gates[gate_id].input2_wire;
        std::uint64_t score1 = this->wire_info.at(input1).mru_step;
        std::uint64_t score2 = this->wire_info.at(input2).mru_step;
        return -(std::int64_t) std::max(score1, score2);
    }
};

static CircuitGate mkgate(WireID input1, WireID input2) {
    CircuitGate gate;
    gate.input1_wire = input1;
    gate.input2_wire = input2;
    gate.type = GateType::AND;
    gate.dead = false;
    return gate;
}

BOOST_AUTO_TEST_CASE(test_mru_traversal) {
    std::vector<WireID> linearization;
    std::unique_ptr<TraversalWriter> w(new VectorStreamWriter<WireID>(&linearization));
    struct {
        Circuit c;
        CircuitGate gates[12];
    } buf;
    Circuit* c = reinterpret_cast<Circuit*>(&buf);
    c->header.num_gates = 12;
    c->header.num_party1_inputs = 4;
    c->header.num_party2_inputs = 4;
    c->header.num_outputs = 4;
    c->header.magic = circuit_magic;
    c->gates[0] = mkgate(0, 1);
    c->gates[1] = mkgate(2, 3);
    c->gates[2] = mkgate(4, 5);
    c->gates[3] = mkgate(6, 7);
    c->gates[4] = mkgate(8, 9);
    c->gates[5] = mkgate(8, 9);
    c->gates[6] = mkgate(10, 11);
    c->gates[7] = mkgate(10, 11);
    c->gates[8] = mkgate(12, 13);
    c->gates[9] = mkgate(12, 13);
    c->gates[10] = mkgate(12, 14);
    c->gates[11] = mkgate(12, 15);

    WireGraph wg(*c);
    TestMRUTraversal traversal(wg, std::move(w), *c);
    traversal.traverse();

    /* Not unique -- sorted by wire ID where ambiguous) */
    std::vector<WireID> expected_order = { 0, 1, 8, 2, 3, 9, 12, 13, 16, 17, 4, 5, 10, 6, 7, 11, 14, 18, 15, 19 };

    BOOST_REQUIRE(linearization.size() == expected_order.size());
    for (int i = 0; i != linearization.size(); i++) {
        BOOST_CHECK(linearization[i] == expected_order[i]);
    }
}
