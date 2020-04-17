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

#include "dsl/graph.hpp"
#include <cassert>
#include <memory>

namespace mage::dsl {
    Graph* Graph::current_working_graph = nullptr;

    Graph::Graph() {
    }

    Graph::~Graph() {
        if (Graph::current_working_graph == this) {
            Graph::current_working_graph = nullptr;
        }
    }

    Graph* Graph::set_current_working_graph(Graph* cwg) {
        Graph* old_cwg = Graph::current_working_graph;
        Graph::current_working_graph = cwg;
        return old_cwg;
    }

    Graph* Graph::get_current_working_graph() {
        return Graph::current_working_graph;
    }
}
