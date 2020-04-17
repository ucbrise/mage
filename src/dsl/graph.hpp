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

#ifndef MAGE_DSL_GRAPH_HPP_
#define MAGE_DSL_GRAPH_HPP_

#include <cstdint>
#include <memory>
#include <vector>

namespace mage::dsl {
    using VertexID = std::uint64_t;
    using BitWidth = std::uint16_t;
    using BitOffset = std::uint8_t;
    enum class Operation : std::uint8_t {
        Undefined = 0,
        Input,
        PublicConstant,
        IntAdd,
        IntIncrement,
        IntSub,
        IntDecrement,
        IntLess,
        Equal,
        IsZero,
        NonZero,
        BitNOT,
        BitAND,
        BitOR,
        BitXOR,
        BitSelect,
        ValueSelect,
        Swap
    };

    const constexpr VertexID invalid_vertex = UINT64_MAX;

    struct Vertex {
        VertexID input1;
        VertexID input2;
        VertexID input3;
        Operation operation;
        BitWidth width;
        union {
            std::uint32_t constant;
            struct {
                BitOffset offset1;
                BitOffset offset2;
                BitOffset offset3;
            };
        };
    };

    class Graph {
    public:
        Graph();
        ~Graph();

        VertexID new_vertex(Operation op, BitWidth width, std::uint32_t constant = 0) {
            Vertex& v = this->vertices.emplace_back();
            v.input1 = invalid_vertex;
            v.input2 = invalid_vertex;
            v.input3 = invalid_vertex;
            v.operation = op;
            v.width = width;
            v.constant = constant;
            return this->vertices.size();
        }

        VertexID new_vertex(Operation op, BitWidth width, VertexID arg0, BitOffset offset0, VertexID arg1 = invalid_vertex, BitOffset offset1 = 0, VertexID arg2 = invalid_vertex, BitOffset offset2 = 0) {
            Vertex& v = this->vertices.emplace_back();
            v.input1 = arg0;
            v.input2 = arg1;
            v.input3 = arg2;
            v.operation = op;
            v.width = width;
            v.offset1 = offset0;
            v.offset2 = offset1;
            v.offset3 = offset2;
            return this->vertices.size();
        }

        void mark_output(VertexID v) {
            this->outputs.push_back(v);
        }

        std::uint64_t num_vertices() {
            return this->vertices.size();
        }

        static Graph* set_current_working_graph(Graph* cwg);
        static Graph* get_current_working_graph();

    private:
        std::vector<Vertex> vertices;
        std::vector<VertexID> outputs;
        static Graph* current_working_graph;
    };
}

#endif
