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
#include <unordered_map>
#include <unordered_set>
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

    KahnTraversal::KahnTraversal(const WireGraph& graph, std::unique_ptr<TraversalWriter>&& out)
        : wg(graph), output(std::move(out)) {
    }

    void KahnTraversal::traverse() {
        WireID evaluated;
        while (this->select_ready_gate(evaluated)) {
            this->output->append(evaluated);
            auto pair = this->wg.outputs_of(evaluated);
            const WireID* enabled = pair.first;
            std::uint64_t num_enabled = pair.second;
            this->mark_inputs_ready(evaluated, enabled, num_enabled);
        }
    }

    FIFOKahnTraversal::FIFOKahnTraversal(const WireGraph& graph, std::unique_ptr<TraversalWriter>&& out)
        : KahnTraversal(graph, std::move(out)), one_input_ready(graph.get_num_wires()) {
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
        return true;
    }

    void FIFOKahnTraversal::mark_inputs_ready(WireID input, const WireID* outputs, std::uint64_t num_outputs) {
        for (std::uint64_t i = 0; i != num_outputs; i++) {
            WireID output = outputs[i];
            if (this->one_input_ready[output]) {
                this->one_input_ready[output] = false;
                this->ready_gate_outputs.push(output);
            } else {
                this->one_input_ready[output] = true;
            }
        }
    }

    WorkingSetTraversal::WorkingSetTraversal(const WireGraph& graph, std::unique_ptr<TraversalWriter>&& out, const Circuit& c)
        : KahnTraversal(graph, std::move(out)), circuit(c) {
        for (WireID i = 0; i != graph.get_num_input_wires(); i++) {
            this->ready_gate_outputs_harmful.insert(i);
        }
    }

    bool pop_item(std::unordered_set<WireID>& set, WireID& item) {
        if (!set.empty()) {
            auto iter = set.begin();
            item = *iter;
            set.erase(iter);
            return true;
        }
        return false;
    }

    bool WorkingSetTraversal::select_ready_gate(WireID& gate_output) {
        bool rv = pop_item(this->ready_gate_outputs_preferred, gate_output)
            || pop_item(this->ready_gate_outputs_harmless, gate_output)
            || pop_item(this->ready_gate_outputs_harmful, gate_output);

        if (rv) {
            /*
             * If we just obtained an input wire, that doesn't actually make
             * it more advantageous to bring in any more wires, so we can skip
             * this step. That's why there's no "else" clause.
             */
            if (gate_output >= this->circuit.get_num_input_wires()) {
                GateID gate_id = this->circuit.output_wire_to_gate(gate_output);
                const CircuitGate& gate = this->circuit.gates[gate_id];
                assert(!gate.dead);
                this->decrement_score(gate.input1_wire);
                this->decrement_score(gate.input2_wire);
            }
        }

        return rv;
    }

    void WorkingSetTraversal::decrement_score(WireID input) {
        auto iter = this->unfired_gate_count.find(input);
        assert(iter != this->unfired_gate_count.end());
        iter->second--;
        if (iter->second == 0) {
            this->unfired_gate_count.erase(iter);
        } else if (iter->second == 1) {
            /* Update any gates that this wire feeds into */
            auto pair = wg.outputs_of(input);
            const WireID* enabled = pair.first;
            std::uint64_t num_enabled = pair.second;
            for (std::uint64_t i = 0; i != num_enabled; i++) {
                WireID gate_output = enabled[i];
                auto j = this->ready_gate_outputs_harmful.find(gate_output);
                if (j != this->ready_gate_outputs_harmful.end()) {
                    this->ready_gate_outputs_harmful.erase(j);
                    this->ready_gate_outputs_harmless.insert(gate_output);
                } else {
                    j = this->ready_gate_outputs_harmless.find(gate_output);
                    if (j != this->ready_gate_outputs_harmless.end()) {
                        this->ready_gate_outputs_harmless.erase(j);
                        this->ready_gate_outputs_preferred.insert(gate_output);
                    } else {
                        assert(this->ready_gate_outputs_preferred.find(gate_output) == this->ready_gate_outputs_preferred.end());
                    }
                }
            }
        }
    }

    void WorkingSetTraversal::mark_inputs_ready(WireID input, const WireID* outputs, std::uint64_t num_outputs) {
        assert(input < this->circuit.get_num_wires());
        if (input >= this->circuit.get_num_wires() - this->circuit.header.num_outputs) {
            return;
        }
        assert(num_outputs != 0);
        assert(this->unfired_gate_count.find(input) == this->unfired_gate_count.end());
        this->unfired_gate_count[input] = num_outputs;
        for (std::uint64_t i = 0; i != num_outputs; i++) {
            WireID output = outputs[i];
            auto iter = this->one_input_ready.find(output);
            if (iter == this->one_input_ready.end()) {
                this->one_input_ready[output] = input;
            } else {
                WireID other_input = iter->second;
                std::uint64_t other_num_outputs = this->unfired_gate_count.at(other_input);
                if (num_outputs == 1 && other_num_outputs == 1) {
                    this->ready_gate_outputs_preferred.insert(output);
                } else if (num_outputs == 1 || other_num_outputs == 1) {
                    this->ready_gate_outputs_harmless.insert(output);
                } else {
                    this->ready_gate_outputs_harmful.insert(output);
                }
                this->one_input_ready.erase(iter);
            }
        }
    }
}
