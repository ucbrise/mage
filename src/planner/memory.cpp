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
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "gate.hpp"
#include "plan.hpp"
#include "platform/memory.hpp"

namespace mage::planner {
    std::uint64_t compute_max_working_set_size(Circuit& c) {
        std::uint64_t max_working_set_size = 0;
        assert(c.header.magic == circuit_magic);

        /* At the very end, the working set consists of all wires. */
        std::uint64_t working_set_size = c.header.num_outputs;
        std::vector<bool> working_set(c.get_num_wires(), false);
        for (WireID wire = c.get_num_wires() - c.header.num_outputs; wire != c.get_num_wires(); wire++) {
            working_set[wire] = true;
        }

        /* Now, work backwards. */
        for (GateID i = c.header.num_gates - 1; i != UINT64_MAX; i--) {
            CircuitGate& gate = c.gates[i];
            WireID output_wire = c.gate_to_output_wire(i);
            if (working_set[output_wire]) {
                /* Otherwise, just skip this gate. */
                if (!working_set[gate.input1_wire]) {
                    working_set[gate.input1_wire] = true;
                    working_set_size++;
                }
                if (!working_set[gate.input2_wire]) {
                    working_set[gate.input2_wire] = true;
                    working_set_size++;
                }
                max_working_set_size = std::max(max_working_set_size, working_set_size);
                working_set[output_wire] = false;
                working_set_size--;
                gate.dead = false;
            } else {
                gate.dead = true;
            }
        }

        assert(working_set_size == c.get_num_input_wires());
        for (std::uint64_t i = 0; i != c.get_num_input_wires(); i++) {
            assert(working_set[i]);
        }

        return max_working_set_size;
    }

    void annotate_input(TraversalIndex& next_use, std::unordered_map<WireID, TraversalIndex>& next_access, WireID input, TraversalIndex this_use) {
        auto lookup_result = next_access.insert(std::make_pair(input, this_use));
        if (lookup_result.second) {
            next_use = never_used_again;
        } else {
            auto iter = lookup_result.first;
            next_use = iter->second;
            iter->second = this_use;
        }
    }

    std::uint64_t annotate_traversal(std::string annotated_traversal_filename, const Circuit& c, std::string traversal_filename) {
        platform::MappedFile<WireID> traversal(traversal_filename.c_str(), false);
        std::uint64_t length = traversal.size() / sizeof(WireID);
        assert(traversal.size() % sizeof(WireID) == 0);
        return annotate_traversal(annotated_traversal_filename, c, traversal.mapping(), length);
    }

    std::uint64_t annotate_traversal(std::string annotated_traversal_filename, const Circuit& c, WireID* traversal, std::uint64_t traversal_length) {
        std::uint64_t max_working_set_size = 0;
        platform::MappedFile<AnnotatedTraversalNode> annotated_traversal_file(annotated_traversal_filename.c_str(), traversal_length * sizeof(AnnotatedTraversalNode));
        AnnotatedTraversalNode* annotated_traversal = annotated_traversal_file.mapping();
        std::unordered_map<WireID, TraversalIndex> next_access;
        for (WireID wire = c.get_num_wires() - c.header.num_outputs; wire != c.get_num_wires(); wire++) {
            next_access[wire] = output_wire;
        }
        for (TraversalIndex i = traversal_length - 1; i != UINT64_MAX; i--) {
            WireID gate_output = traversal[i];
            AnnotatedTraversalNode& ann = annotated_traversal[i];
            ann.gate_output = gate_output;

            bool dead;
            if (gate_output < c.get_num_input_wires()) {
                /* We want to include input wires, but they are free swap-ins. */
                ann.next_input1_use = never_used_again;
                ann.next_input2_use = never_used_again;
            } else {
                GateID gate_id = c.output_wire_to_gate(gate_output);
                assert(gate_id < c.header.num_gates);
                const CircuitGate& gate = c.gates[gate_id];
                assert(!gate.dead);
                annotate_input(ann.next_input1_use, next_access, gate.input1_wire, i);
                annotate_input(ann.next_input2_use, next_access, gate.input2_wire, i);
            }
            max_working_set_size = std::max(max_working_set_size, next_access.size());
            auto iter = next_access.find(gate_output);
            assert(iter != next_access.end());
            ann.next_output_use = iter->second;
            next_access.erase(iter);
        }
        return max_working_set_size;
    }

    std::uint64_t simple_allocator(std::unique_ptr<TraversalReader> traversal, std::unique_ptr<PlanWriter> writer, std::uint64_t num_wire_slots, const Circuit& circuit) {
        std::uint64_t num_swaps = 0;

        /* Allows us to grab a free slot for allocation. */
        std::vector<WireMemoryLocation> free;
        free.reserve(num_wire_slots);
        for (std::uint64_t i = num_wire_slots - 1; i != UINT64_MAX; i--) {
            free.push_back(i);
        }

        /* Allows us to keep track of which wires are resident and where. */
        std::unordered_map<WireID, WireMemoryLocation> resident;
        resident.reserve(num_wire_slots);

        /* Buffer the next action in the plan. */
        PlannedAction action;

        WireID gate_output;
        while (traversal->next(gate_output)) {
            GateID gate_id = circuit.output_wire_to_gate(gate_output);
            const CircuitGate& gate = circuit.gates[gate_id];

            WireID gate_input1 = gate.input1_wire;
            WireID gate_input2 = gate.input2_wire;

            auto input1_iter = resident.find(gate_input1);
            if (input1_iter == resident.end()) {
                // TODO: swap in input1
                num_swaps += 2;
            }
            WireMemoryLocation input1_location = input1_iter->second;

            // TODO: if this is the last use of input1, mark its location free

            auto input2_iter = resident.find(gate_input2);
            if (input2_iter == resident.end()) {
                // TODO: swap in input2
                num_swaps += 2;
            }
            WireMemoryLocation input2_location = input2_iter->second;

            // TODO: if this is the last use of input2, mark its location free

            if (free.empty()) {
                // TODO: swap something out so that we have a free space
                num_swaps++;
            }

            WireMemoryLocation output_location = free.back();
            free.pop_back();

            // Emit execute action
            action.opcode = PlannedActionType::GateExecTable;
            action.exec.input1 = input1_location;
            action.exec.input2 = input2_location;
            action.exec.output = output_location;
            writer->append(action);
        }

        return num_swaps;
    }

    Allocator::Allocator(std::unique_ptr<AnnotatedTraversalReader>&& annotated, std::unique_ptr<PlanWriter>&& out, const Circuit& c)
        : traversal(std::move(annotated)), output(std::move(out)), circuit(c), num_swapouts(0), num_swapins(0) {
    }

    void Allocator::emit_swapout(WireMemoryLocation primary, WireStorageLocation secondary) {
        PlannedAction action;
        action.opcode = PlannedActionType::SwapOut;
        action.swapout.primary = primary;
        action.swapout.secondary = secondary;
        this->output->append(action);
        this->num_swapouts++;
    }

    void Allocator::emit_swapin(WireStorageLocation secondary, WireMemoryLocation primary) {
        PlannedAction action;
        action.opcode = PlannedActionType::SwapIn;
        action.swapout.secondary = secondary;
        action.swapout.primary = primary;
        this->output->append(action);
        this->num_swapins++;
    }

    void Allocator::allocate() {
        AnnotatedTraversalNode node;
        while (this->traversal->next(node)) {
            RawGate gate;
            gate.output = node.gate_output;
            if (gate.output < this->circuit.get_num_input_wires()) {
                continue;
            }

            GateID gate_id = this->circuit.output_wire_to_gate(gate.output);
            const CircuitGate& circuit_gate = this->circuit.gates[gate_id];
            gate.input1 = circuit_gate.input1_wire;
            gate.input2 = circuit_gate.input2_wire;

            static PlannedAction action;
            switch (circuit_gate.type) {
            case GateType::NOT:
            case GateType::XOR:
                action.opcode = PlannedActionType::GateExecXOR;
                break;
            case GateType::XNOR:
                action.opcode = PlannedActionType::GateExecXNOR;
                break;
            default:
                action.opcode = PlannedActionType::GateExecTable;
                break;
            }

            this->allocate_gate(action.exec, gate, node);
            this->output->append(action);
        }
    }

    std::uint64_t Allocator::get_num_swapouts() {
        return this->num_swapouts;
    }

    std::uint64_t Allocator::get_num_swapins() {
        return this->num_swapins;
    }

    SimpleAllocator::SimpleAllocator(std::unique_ptr<AnnotatedTraversalReader>&& annotated, std::unique_ptr<PlanWriter>&& out, const Circuit& c, std::uint64_t num_wire_slots)
        : Allocator(std::move(annotated), std::move(out), c), size(num_wire_slots) {
        /* Initially, all in-memory slots are free. */
        this->free.reserve(num_wire_slots);
        for (std::uint64_t i = num_wire_slots - 1; i != UINT64_MAX; i--) {
            this->free.push_back(i);
        }
        this->resident.reserve(num_wire_slots);
    }

    WireMemoryLocation SimpleAllocator::allocate_slot() {
        WireMemoryLocation slot;
        if (this->free.empty()) {
            slot = this->evict_wire();
        } else {
            slot = this->free.back();
            this->free.pop_back();
        }
        return slot;
    }

    bool SimpleAllocator::find_gate_input(WireMemoryLocation& slot, WireID input) {
        auto iter = this->resident.find(input);
        if (iter == this->resident.end()) {
            /* The input wire is not resident so we have to swap it in. */
            slot = this->allocate_slot();
            this->emit_swapin(input, slot);
            return true;
        }

        slot = iter->second;
        iter->second = this->size; // "Pins" this so it isn't evicted.
        return false;
    }

    bool SimpleAllocator::update_residency_state(WireMemoryLocation slot, WireID wire, bool just_swapped_in, bool final_use) {
        assert((this->resident.find(wire) == this->resident.end()) == just_swapped_in);
        if (final_use) {
            if (!just_swapped_in) {
                this->resident.erase(wire);
            }
            this->free.push_back(slot);
            return false;
        } else if (just_swapped_in) {
            this->resident[wire] = slot;
        } else {
            /* Needed since we "pinned" this entry and need to reset its this->resident value. */
            this->resident[wire] = slot;
        }
        return true;
    }

    void SimpleAllocator::allocate_gate(GateExecAction& slots, const RawGate& gate, const AnnotatedTraversalNode& annotation) {
        bool input1_swapin = this->find_gate_input(slots.input1, gate.input1);
        bool input2_swapin;
        if (gate.input1 == gate.input2) {
            slots.input2 = slots.input1;
        } else {
            input2_swapin = this->find_gate_input(slots.input2, gate.input2);
        }

        bool input1_final = (annotation.next_input1_use == never_used_again);
        bool input2_final = (annotation.next_input2_use == never_used_again);

        this->update_residency_state(slots.input1, gate.input1, input1_swapin, input1_final);
        if (gate.input1 != gate.input2) {
            this->update_residency_state(slots.input2, gate.input2, input2_swapin, input2_final);
        }

        slots.output = this->allocate_slot();

        this->resident[gate.output] = slots.output;
    }

    WireMemoryLocation SimpleAllocator::evict_wire() {
        /*
         * Chooses a slot, among those in resident, to evict. A better
         * allocator would choose the slot more carefully. It must NOT choose
         * pinned slot (one that resident maps to this->size).
         */
        auto iter = this->resident.begin();
        assert(iter != this->resident.end());
        while (iter->second == this->size) {
            iter++;
            assert(iter != this->resident.end());
        }
        this->emit_swapout(iter->second, iter->first);
        WireMemoryLocation slot = iter->second;
        this->resident.erase(iter);
        return slot;
    }

    BeladyAllocator::BeladyAllocator(std::unique_ptr<AnnotatedTraversalReader>&& annotated, std::unique_ptr<PlanWriter>&& out, const Circuit& c, std::uint64_t num_wire_slots)
        : SimpleAllocator(std::move(annotated), std::move(out), c, num_wire_slots) {
    }

    void BeladyAllocator::insert_next_use_order(WireID wire, WireMemoryLocation slot, TraversalIndex next_use) {
        assert(next_use != never_used_again);
        this->next_use_by_slot[slot] = next_use;
        this->next_use_order.insert(std::make_pair(next_use, std::make_pair(slot, wire)));
    }

    void BeladyAllocator::update_next_use_order(WireID wire, WireMemoryLocation slot, TraversalIndex next_use) {
        auto i = this->next_use_by_slot.find(slot);
        if (i != this->next_use_by_slot.end()) {
            auto j = this->next_use_order.find(i->second);
            assert(j != this->next_use_order.end());
            while (j->second.first != slot) {
                j++;
                assert(j->first == i->second);
            }
            assert(j->second.second == wire);
            this->next_use_order.erase(j);
            this->next_use_by_slot.erase(i);
        }
        if (next_use != never_used_again) {
            this->insert_next_use_order(wire, slot, next_use);
        }
    }

    void BeladyAllocator::allocate_gate(GateExecAction& slots, const RawGate& gate, const AnnotatedTraversalNode& annotation) {
        this->SimpleAllocator::allocate_gate(slots, gate, annotation);
        this->update_next_use_order(gate.input1, slots.input1, annotation.next_input1_use);
        if (gate.input1 == gate.input2) {
            assert(slots.input1 == slots.input2);
        } else {
            this->update_next_use_order(gate.input2, slots.input2, annotation.next_input2_use);
        }
        this->insert_next_use_order(gate.output, slots.output, annotation.next_output_use);
    }

    WireMemoryLocation BeladyAllocator::evict_wire() {
        auto iter = this->next_use_order.end();

        do {
            assert(iter != this->next_use_order.begin());
            iter--;
        } while (this->resident.at(iter->second.second) == this->size);

        WireMemoryLocation slot = iter->second.first;
        WireID wire = iter->second.second;
        this->emit_swapout(slot, wire);
        this->next_use_order.erase(iter);
        this->next_use_by_slot.erase(slot);
        this->resident.erase(wire);
        return slot;
    }
}
