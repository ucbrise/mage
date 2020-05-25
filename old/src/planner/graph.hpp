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

#include <utility>

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
}

#endif
