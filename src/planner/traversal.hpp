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

#ifndef MAGE_PLANNER_TRAVERSAL_HPP_
#define MAGE_PLANNER_TRAVERSAL_HPP_

#include <cstdint>
#include <forward_list>
#include <fstream>
#include <iostream>
#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "circuit.hpp"
#include "stream.hpp"
#include "planner/graph.hpp"
#include "util/prioqueue.hpp"

namespace mage::planner {
    using TraversalWriter = StreamWriter<WireID>;
    using TraversalReader = StreamReader<WireID>;
    using FileTraversalWriter = FileStreamWriter<WireID>;
    using FileTraversalReader = FileStreamReader<WireID>;

    /*
     * Write out the Traversal concept once we have a C++20 compiler.
     */

    class NopTraversal {
    public:
        NopTraversal(const Circuit& c, std::unique_ptr<TraversalWriter>&& out);
        void traverse();

    private:
        const Circuit& circuit;
        std::unique_ptr<TraversalWriter> output;
    };

    class KahnTraversal {
    protected:
        KahnTraversal(const WireGraph& wg, std::unique_ptr<TraversalWriter>&& out);
        virtual ~KahnTraversal() = 0;
        virtual bool select_ready_gate(WireID& gate_output) = 0;
        virtual void mark_inputs_ready(WireID input, const WireID* outputs, std::uint64_t num_outputs) = 0;

        const WireGraph& wg;

    public:
        void traverse();

    private:
        std::unique_ptr<TraversalWriter> output;
    };

    class FIFOKahnTraversal : public KahnTraversal {
    public:
        FIFOKahnTraversal(const WireGraph& graph, std::unique_ptr<TraversalWriter>&& out);
        bool select_ready_gate(WireID& gate_output) override;
        void mark_inputs_ready(WireID input, const WireID* outputs, std::uint64_t num_outputs) override;

    private:
        std::queue<WireID> ready_gate_outputs;
        std::vector<bool> one_input_ready;
    };

    class WorkingSetTraversal : public KahnTraversal {
    public:
        WorkingSetTraversal(const WireGraph& graph, std::unique_ptr<TraversalWriter>&& out, const Circuit& c);
        bool select_ready_gate(WireID& gate_output) override;
        void mark_inputs_ready(WireID input, const WireID* outputs, std::uint64_t num_outputs) override;

    private:
        void decrement_score(WireID input);

        /*
         * The score of a wire is "unfired gate count" --- the number of
         * unfired gates it feeds into. this->unfired_gate_count keeps track of
         * the score of each wire whose corresponding gate has been executed
         * and whose score is nonzero.
         *
         * An unfired gate is PREFERRED if both of its input wires have score
         * 1. It is HARMLESS if at least one of its input wires has score 1. It
         * is HARMFUL if neither of its input wires has score 1.
         */

        /* Ready gates whose execution would reduce the working set size. */
        std::unordered_set<WireID> ready_gate_outputs_preferred;

        /* Ready gates whose execution would not change the working set size. */
        std::unordered_set<WireID> ready_gate_outputs_harmless;

        /* Ready gates whose execution would increase the working set size. */
        std::unordered_set<WireID> ready_gate_outputs_harmful;

        /* Maps an input wire to the number of unfired gates it feeds into. */
        std::unordered_map<WireID, std::uint64_t> unfired_gate_count;

        /*
         * Maps gate output wires where one input is ready to the input wire
         * that is ready.
         */
        std::unordered_map<WireID, WireID> one_input_ready;

        const Circuit& circuit;
    };

    struct WireInfo {
        WireInfo(std::uint64_t fanout, std::uint64_t step) : unfired_gate_count(fanout), mru_step(step) {
        }
        std::uint64_t unfired_gate_count;
        std::uint64_t mru_step;
    };

    class LinearMRUTraversal : public KahnTraversal {
    public:
        LinearMRUTraversal(const WireGraph& wg, std::unique_ptr<TraversalWriter>&& out, const Circuit& c);
        bool select_ready_gate(WireID& gate_output) override;
        void mark_inputs_ready(WireID input, const WireID* outputs, std::uint64_t num_outputs) override;

    private:
        std::uint64_t current_step;
        std::forward_list<WireID> ready_gate_outputs;
        std::vector<bool> one_input_ready;
        std::unordered_map<WireID, WireInfo> wire_info;
        const Circuit& circuit;
    };

    class MRUTraversal : public KahnTraversal {
    public:
        MRUTraversal(const WireGraph& wg, std::unique_ptr<TraversalWriter>&& out, const Circuit& c);
        bool select_ready_gate(WireID& gate_output) override;
        void mark_inputs_ready(WireID input, const WireID* outputs, std::uint64_t num_outputs) override;

    private:
        void updated_wire_mru_step(WireID wire, std::uint64_t new_mru_step);
        std::uint64_t compute_output_score(WireID gate_output);
        std::uint64_t current_step;

        /* Priority queue, ordered by negative score */
        PriorityQueue<std::int64_t, WireID> ready_gate_outputs;
        std::unordered_map<WireID, WireInfo> wire_info;
        std::vector<bool> one_input_ready;
        const Circuit& circuit;
    };
}

#endif
