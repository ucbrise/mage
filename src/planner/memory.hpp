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

#ifndef MAGE_PLANNER_MEMORY_HPP_
#define MAGE_PLANNER_MEMORY_HPP_

#include <cstdint>

#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "circuit.hpp"
#include "plan.hpp"
#include "stream.hpp"
#include "planner/traversal.hpp"
#include "platform/memory.hpp"

namespace mage::planner {
    std::uint64_t compute_max_working_set_size(Circuit& c);

    using TraversalIndex = std::uint64_t;
    const constexpr TraversalIndex never_used_again = 0;
    const constexpr TraversalIndex output_wire = UINT64_MAX;
    struct AnnotatedTraversalNode {
        WireID gate_output;
        TraversalIndex next_input1_use;
        TraversalIndex next_input2_use;
        TraversalIndex next_output_use;
    };

    using AnnotatedTraversalReader = StreamReader<AnnotatedTraversalNode>;
    using AnnotatedTraversalWriter = StreamWriter<AnnotatedTraversalNode>;
    using FileAnnotatedTraversalReader = FileStreamReader<AnnotatedTraversalNode>;
    using FileAnnotatedTraversalWriter = FileStreamWriter<AnnotatedTraversalNode>;

    std::uint64_t annotate_traversal(std::string annotated_traversal_filename, const Circuit& c, std::string traversal_filename);
    std::uint64_t annotate_traversal(std::string annotated_traversal_filename, const Circuit& c, WireID* traversal, std::uint64_t traversal_length);

    struct RawGate {
        WireID input1;
        WireID input2;
        WireID output;
    };

    class Allocator {
    protected:
        Allocator(std::unique_ptr<AnnotatedTraversalReader>&& annotated, std::unique_ptr<PlanWriter>&& out, const Circuit& c);
        virtual void allocate_gate(GateExecAction& slots, const RawGate& gate, const AnnotatedTraversalNode& annotation) = 0;
        void emit_swapout(WireMemoryLocation primary, WireStorageLocation secondary);
        void emit_swapin(WireStorageLocation secondary, WireMemoryLocation primary);

        std::unique_ptr<AnnotatedTraversalReader> traversal;
        std::unique_ptr<PlanWriter> output;
        const Circuit& circuit;

    public:
        void allocate();
        std::uint64_t get_num_swapouts();
        std::uint64_t get_num_swapins();

    private:
        /* Keeps track of the number of swaps performed in the allocation. */
        std::uint64_t num_swapouts;
        std::uint64_t num_swapins;
    };

    class SimpleAllocator : public Allocator {
    public:
        SimpleAllocator(std::unique_ptr<AnnotatedTraversalReader>&& annotated, std::unique_ptr<PlanWriter>&& out, const Circuit& c, std::uint64_t num_wire_slots);
        void allocate_gate(GateExecAction& slots, const RawGate& gate, const AnnotatedTraversalNode& annotation) override;

    protected:
        WireMemoryLocation allocate_slot();
        bool find_gate_input(WireMemoryLocation& slot, WireID input);
        virtual bool update_residency_state(WireMemoryLocation slot, WireID wire, bool just_swapped_in, bool final_use);
        virtual WireMemoryLocation evict_wire();

        /* Number of wire slots in memory on target machine. */
        std::uint64_t size;

        /* Allows us to quickly find a free slot if one exists. */
        std::vector<WireMemoryLocation> free;

        /*
         * Allows us to quickly check if a wire is resident and, if so,
         * determine its in-memory location.
         */
        std::unordered_map<WireID, WireMemoryLocation> resident;
    };

    class BeladyAllocator : public SimpleAllocator {
    public:
        BeladyAllocator(std::unique_ptr<AnnotatedTraversalReader>&& annotated, std::unique_ptr<PlanWriter>&& out, const Circuit& c, std::uint64_t num_wire_slots);
        void allocate_gate(GateExecAction& slots, const RawGate& gate, const AnnotatedTraversalNode& annotation) override;
        WireMemoryLocation evict_wire() override;

    private:
        void insert_next_use_order(WireID wire, WireMemoryLocation slot, TraversalIndex next_use);
        void update_next_use_order(WireID wire, WireMemoryLocation slot, TraversalIndex next_use);

        std::multimap<TraversalIndex, std::pair<WireMemoryLocation, WireID>> next_use_order;
        std::unordered_map<WireMemoryLocation, TraversalIndex> next_use_by_slot;
    };
}

#endif
