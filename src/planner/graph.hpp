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

#ifndef MAGE_PLANNER_GRAPH_HPP_
#define MAGE_PLANNER_GRAPH_HPP_

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <queue>
#include <utility>
#include <vector>
#include "circuit.hpp"

namespace mage::planner {
    /*
     * In a WireGraph, each vertex is a wire. There is an edge from w1 to w2 if
     * there exists a gate such that w1 is an input and w2 is an output. If
     * both inputs to the gate are w1, then there are two such edges.
     */
    class WireGraph {
    public:
        WireGraph(const Circuit& c);
        ~WireGraph();

        std::pair<const WireID*, std::ptrdiff_t> outputs_of(WireID wire) const;
        std::uint64_t get_num_wires() const;
        std::uint64_t get_num_input_wires() const;

    private:
        WireID* neighbors;
        WireID** adjacency;
        std::uint64_t neighbors_len;
        std::uint64_t adjacency_len;
        std::uint64_t num_wires;
        std::uint64_t num_input_wires;
    };

    class KahnTraversal {
    protected:
        KahnTraversal(const WireGraph& wg);
        virtual bool select_ready_gate(WireID& gate_output) = 0;
        virtual void mark_input_ready(WireID output) = 0;

    public:
        void traverse();

    private:
        const WireGraph& wg;
    };

    class TraversalWriter {
    public:
        virtual void append(WireID gate_output) = 0;
    };

    class TraversalReader {
    public:
        virtual bool next(WireID& gate_output) = 0;
    };

    class FileTraversalWriter : public TraversalWriter {
    public:
        FileTraversalWriter(std::string filename);
        void append(WireID gate_output) override;

    private:
        std::ofstream output;
    };

    class FileTraversalReader : public TraversalReader {
    public:
        FileTraversalReader(std::string filename);
        bool next(WireID& gate_output) override;

    private:
        std::ifstream input;
    };

    class FIFOKahnTraversal : public KahnTraversal {
    public:
        FIFOKahnTraversal(const WireGraph& wg, std::unique_ptr<TraversalWriter>&& out);
        bool select_ready_gate(WireID& gate_output) override;
        void mark_input_ready(WireID output) override;

    private:
        std::queue<WireID> ready_gate_outputs;
        std::vector<bool> one_input_ready;
        std::unique_ptr<TraversalWriter> output;
    };
}

#endif
