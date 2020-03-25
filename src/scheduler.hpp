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

#ifndef MAGE_SCHEDULER_HPP_
#define MAGE_SCHEDULER_HPP_

#include <cstdint>
#include <condition_variable>
#include <memory>
#include <map>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include "gate.hpp"
#include "wire.hpp"
#include "loader/loader.hpp"

namespace mage {
    class GatePageDependencies {
        std::unordered_set<WireGlobalID> inputs;
        std::unordered_set<WireGlobalID> outputs;
        std::uint64_t num_unready_inputs;

    public:
        GatePage* page;

        GatePageDependencies(GatePage* page);

        friend class Scheduler;
    };

    class WireInfo {
        std::unordered_map<GatePageID, GatePageDependencies*> dependents;

        friend class Scheduler;
    };

    /*
     * Keeps track of what gate pages are "in-flight" and what wires they
     * depend on.
     */
    class Scheduler {
    protected:
        std::unordered_map<WireGlobalID, WireInfo> wire_map;
        std::mutex wire_map_mutex;
        std::unordered_map<GatePageID, std::unique_ptr<GatePageDependencies>> executing;
        std::mutex executing_mutex;
        loader::GatePageLoader* loader;

    public:
        virtual void enqueue_ready_gate_page(GatePageID id, std::unique_ptr<GatePageDependencies>&& gates) = 0;
        virtual std::tuple<GatePageID, GatePage*> dequeue_ready_gate_page() = 0;

        void gate_page_loaded(GatePageID id, GatePage* page);
        GatePage* gate_page_executed(GatePageID id);
    };

    class FIFOScheduler : public Scheduler {
        std::map<GatePageID, std::unique_ptr<GatePageDependencies>> ready;
        std::mutex ready_mutex;
        std::condition_variable ready_cond;

    public:
        void enqueue_ready_gate_page(GatePageID id, std::unique_ptr<GatePageDependencies>&& gates);
        std::tuple<GatePageID, GatePage*> dequeue_ready_gate_page();
    };
}

#endif
