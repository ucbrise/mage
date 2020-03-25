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

#include "scheduler.hpp"

#include <condition_variable>
#include <iterator>
#include <memory>
#include <mutex>
#include <tuple>

#include "gate.hpp"
#include "wire.hpp"

namespace mage {
    GatePageDependencies::GatePageDependencies(GatePage* page) {
        const GateStructure* structures = page->structure();
        for (std::uint32_t i = 0; i != page->num_gates; i++) {
            const GateStructure* structure = &structures[i];
            if (structure->input1.is_global()) {
                this->inputs.insert(structure->input1.global_id());
            }
            if (structure->input2.is_global()) {
                this->inputs.insert(structure->input2.global_id());
            }
            if (structure->output.is_global()) {
                this->outputs.insert(structure->output.global_id());
            }
        }
        this->num_unready_inputs = this->inputs.size();
        this->page = page;
    }

    void Scheduler::gate_page_loaded(GatePageID id, GatePage* page) {
        GatePageDependencies* gpd = new GatePageDependencies(page);
        if (gpd->num_unready_inputs == 0) {
            std::unique_ptr<GatePageDependencies> ptr(gpd);
            this->enqueue_ready_gate_page(id, std::move(ptr));
            return;
        }
        {
            std::unique_lock<std::mutex> lock(this->wire_map_mutex);
            for (WireGlobalID wgi : gpd->inputs) {
                this->wire_map[wgi].dependents[id] = gpd;
            }
        }
    }

    GatePage* Scheduler::gate_page_executed(GatePageID id) {
        std::unique_ptr<GatePageDependencies> executed;

        /* Remove gate from executing list. */
        {
            std::unique_lock<std::mutex> lock(this->executing_mutex);
            auto iter = this->executing.find(id);
            executed = std::move(iter->second);
            this->executing.erase(iter);
        }

        /* Find gates that are now ready to run. */
        {
            std::unique_lock<std::mutex> lock(this->executing_mutex);
            for (WireGlobalID wgid : executed->outputs) {
                auto wmiter = this->wire_map.find(wgid);
                auto& enabled = wmiter->second.dependents;
                for (auto dgi = enabled.begin(); dgi != enabled.end(); dgi++) {
                    GatePageDependencies* gpd = dgi->second;
                    gpd->num_unready_inputs--;
                    if (gpd->num_unready_inputs == 0) {
                        std::unique_ptr<GatePageDependencies> ready(gpd);
                        this->enqueue_ready_gate_page(dgi->first, std::move(ready));
                    }
                }
                this->wire_map.erase(wmiter);
            }
        }

        /*
         * The executed gate page will be deleted when the unique pointer goes
         * out of scope. Before then, tell the loader that the page can be
         * stolen.
         */
        this->loader->gate_page_executed(id, executed->page);
        return executed->page;
    }

    void FIFOScheduler::enqueue_ready_gate_page(GatePageID id, std::unique_ptr<GatePageDependencies>&& gates) {
        std::unique_lock<std::mutex> lock(this->ready_mutex);
        this->ready[id] = std::move(gates);
        this->ready_cond.notify_one();
    }

    std::tuple<GatePageID, GatePage*> FIFOScheduler::dequeue_ready_gate_page() {
        std::unique_lock<std::mutex> lock(this->ready_mutex);
        this->ready_cond.wait(lock);
        auto it = this->ready.begin();
        GatePageID id = it->first;
        std::unique_ptr<GatePageDependencies> gpd = std::move(it->second);
        GatePage* page = gpd->page;
        this->executing[id] = std::move(gpd);
        this->ready.erase(it);
        return std::make_tuple(id, page);
    }
}
