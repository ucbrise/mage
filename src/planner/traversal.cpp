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

#include "planner/traversal.hpp"

#include <cstdint>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "graph.hpp"

namespace mage::planner {
    std::vector<GateID> kahn_traversal(WireGraph& wg, std::uint64_t num_input_wires, std::uint64_t num_noninput_wires) {
        std::vector<GateID> linearization;
        linearization.reserve(num_noninput_wires);
        std::vector<bool> one_input_ready(num_noninput_wires);
        std::queue<WireID> both_inputs_ready;
        for (WireID i = 0; i != num_input_wires; i++) {
            both_inputs_ready.push(i);
        }
        while (!both_inputs_ready.empty()) {
            WireID w = both_inputs_ready.front();
            both_inputs_ready.pop();
            if (w > num_input_wires) {
                linearization.push_back(w);
            }

            auto pair = wg.outputs_of(w);
            const WireID* enabled = pair.first;
            std::uint64_t num_enabled = pair.second;
            for (std::uint64_t i = 0; i != num_enabled; i++) {
                WireID output = enabled[i];
                if (one_input_ready[output]) {
                    one_input_ready[output] = false;
                    both_inputs_ready.push(output);
                } else {
                    one_input_ready[output] = true;
                }
            }
        }
        return linearization;
    }

    KahnTraversal::KahnTraversal(const WireGraph& graph) : wg(graph) {
    }

    void KahnTraversal::traverse() {
        WireID evaluated;
        while (this->select_ready_gate(evaluated)) {
            auto pair = this->wg.outputs_of(evaluated);
            const WireID* enabled = pair.first;
            std::uint64_t num_enabled = pair.second;
            for (std::uint64_t i = 0; i != num_enabled; i++) {
                this->mark_input_ready(enabled[i]);
            }
        }
    }

    FIFOKahnTraversal::FIFOKahnTraversal(const WireGraph& graph, std::unique_ptr<TraversalWriter>&& out)
        : KahnTraversal(graph), one_input_ready(graph.get_num_wires()), output(std::move(out)) {
        for (WireID i = 0; i != graph.get_num_input_wires(); i++) {
            this->ready_gate_outputs.push(i);
        }
    }

    bool FIFOKahnTraversal::select_ready_gate(WireID& gate_output) {
        if (this->ready_gate_outputs.empty()) {
            return false;
        }
        gate_output = this->ready_gate_outputs.front();
        this->ready_gate_outputs.pop();
        this->output->append(gate_output);
        return true;
    }

    void FIFOKahnTraversal::mark_input_ready(WireID output) {
        if (this->one_input_ready[output]) {
            this->one_input_ready[output] = false;
            this->ready_gate_outputs.push(output);
        } else {
            this->one_input_ready[output] = true;
        }
    }
}
