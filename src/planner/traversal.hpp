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
#include <fstream>
#include <iostream>
#include <memory>
#include <queue>
#include <utility>
#include <vector>
#include "circuit.hpp"
#include "stream.hpp"
#include "planner/graph.hpp"

namespace mage::planner {
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

    using TraversalWriter = StreamWriter<WireID>;
    using TraversalReader = StreamReader<WireID>;
    using FileTraversalWriter = FileStreamWriter<WireID>;
    using FileTraversalReader = FileStreamReader<WireID>;

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
