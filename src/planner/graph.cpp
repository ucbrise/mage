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

#include "graph.hpp"

#include <cassert>
#include <cstdint>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "platform/memory.hpp"

namespace mage::planner {
    WireGraph::WireGraph(const Circuit& c) : num_wires(c.get_num_wires()), num_input_wires(c.get_num_input_wires()) {
        /* Each gate is a neighbor to at most two other gates. */
        std::uint64_t num_noninput_wires = this->num_wires - this->num_input_wires;
        assert(num_noninput_wires == c.header.num_gates);
        this->neighbors_len = std::max(num_noninput_wires * 2, this->num_wires) * sizeof(WireID);
        this->neighbors = platform::allocate_resident_memory<WireID>(this->neighbors_len, true);

        /* Pointer to adjacency list for each gate. */
        this->adjacency_len = (this->num_wires + 1) * sizeof(WireID*);
        this->adjacency = platform::allocate_resident_memory<WireID*>(this->adjacency_len, true);

        /* Determine the fanout of each wire. */
        for (GateID i = 0; i != c.header.num_gates; i++) {
            const CircuitGate* gate = &c.gates[i];
            if (!gate->dead) {
                this->neighbors[gate->input1_wire]++;
                this->neighbors[gate->input2_wire]++;
            }
        }

        /*
         * Now, compute the adjacency list pointers. The pointer at index i + 1
         * points to the adjacency list for wire i.
         */
        this->adjacency[1] = &this->neighbors[0];
        for (WireID i = 1; i != this->num_wires; i++) {
            this->adjacency[i + 1] = this->adjacency[i] + this->neighbors[i - 1];
        }

        /* Find the edges on the WireGraph. */
        for (GateID i = 0; i != c.header.num_gates; i++) {
            const CircuitGate* gate = &c.gates[i];
            if (!gate->dead) {
                std::uint64_t** adj1 = &this->adjacency[gate->input1_wire + 1];
                **adj1 = c.gate_to_output_wire(i);
                (*adj1)++;

                std::uint64_t** adj2 = &this->adjacency[gate->input2_wire + 1];
                **adj2 = c.gate_to_output_wire(i);
                (*adj2)++;
            }
        }

        /*
         * Now, the pointer at index i + 1 points to the adjacency list for
         * wire i + 1.
         */
        this->adjacency[0] = &this->neighbors[0];
    }

    WireGraph::~WireGraph() {
        platform::deallocate_resident_memory(this->adjacency, this->adjacency_len);
        platform::deallocate_resident_memory(this->neighbors, this->neighbors_len);
    }

    std::pair<const WireID*, std::ptrdiff_t> WireGraph::outputs_of(std::uint64_t wire) const {
        return std::make_pair(this->adjacency[wire], this->adjacency[wire + 1] - this->adjacency[wire]);
    }

    std::uint64_t WireGraph::get_num_wires() const {
        return this->num_wires;
    }

    std::uint64_t WireGraph::get_num_input_wires() const {
        return this->num_input_wires;
    }

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

    FileTraversalWriter::FileTraversalWriter(std::string filename) {
        this->output.exceptions(std::ios::failbit | std::ios::badbit);
        this->output.open(filename, std::ios::out | std::ios::binary | std::ios::trunc);
    }

    void FileTraversalWriter::append(WireID gate_output) {
        this->output.write(reinterpret_cast<char*>(&gate_output), sizeof(output));
    }

    FileTraversalReader::FileTraversalReader(std::string filename) {
        this->input.exceptions(std::ios::failbit | std::ios::badbit);
        this->input.open(filename, std::ios::in | std::ios::binary);
    }

    bool FileTraversalReader::next(WireID& gate_output) {
        this->input.read(reinterpret_cast<char*>(&gate_output), sizeof(gate_output));
        return this->input.good();
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
