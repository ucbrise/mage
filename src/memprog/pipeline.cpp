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

#include "memprog/pipeline.hpp"
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include "memprog/annotation.hpp"
#include "memprog/placement.hpp"
#include "memprog/program.hpp"
#include "memprog/replacement.hpp"
#include "memprog/scheduling.hpp"

namespace mage::memprog {
    DefaultPipeline::DefaultPipeline(const std::string& name) : Pipeline(name),
        page_shift(12), num_pages(1 << 10), prefetch_buffer_size(256), prefetch_lookahead(10000),
        stats({}), verbose(false) {
    }

    DefaultPipeline::DefaultPipeline(const std::string& name, const util::ConfigValue& worker) : Pipeline(name) {
        this->read_config(worker);
    }

    void DefaultPipeline::set_verbose(bool be_verbose) {
        this->verbose = be_verbose;
    }

    void DefaultPipeline::read_config(const util::ConfigValue& worker) {
        this->page_shift = worker["page_shift"].as_int();
        this->num_pages = worker["num_pages"].as_int();
        this->prefetch_buffer_size = worker["prefetch_buffer_size"].as_int();
        this->prefetch_lookahead = worker["prefetch_lookahead"].as_int();
    }

    void DefaultPipeline::program(Program<BinnedPlacer>** p, PlacementPlugin plugin, std::function<void()> dsl_program, const std::string& prog_file) {
        Program<BinnedPlacer> program(prog_file, this->page_shift, plugin);
        *p = &program;
        dsl_program();
        *p = nullptr;
        this->stats.num_instructions = program.num_instructions();

        if (this->verbose) {
            std::cout << "Created program with " << program.num_instructions() << " instructions" << std::endl;
        }
    }

    void DefaultPipeline::allocate(const std::string& prog_file, const std::string& repprog_file) {
        this->progress_bar.set_label("Annotations Pass");
        std::string ann_file = this->program_name + ".ann";
        annotate_program(ann_file, prog_file, this->page_shift, &this->progress_bar);
        this->progress_bar.finish();
        if (this->verbose) {
            std::cout << "Computed annotations" << std::endl;
        }

        this->progress_bar.set_label("Replacement Pass");
        BeladyAllocator allocator(repprog_file, prog_file, ann_file, this->num_pages, this->page_shift);
        allocator.allocate(&this->progress_bar);
        this->progress_bar.finish();
        this->stats.num_swapouts = allocator.get_num_swapouts();
        this->stats.num_swapins = allocator.get_num_swapins();
        if (this->verbose) {
            std::cout << "Finished replacement stage: " << allocator.get_num_swapouts() << " swapouts, " << allocator.get_num_swapins() << " swapins" << std::endl;
        }
    }

    void DefaultPipeline::schedule(const std::string& repprog_file, const std::string& memprog_file) {
        this->progress_bar.set_label("Scheduling Pass");
        BackdatingScheduler scheduler(repprog_file, memprog_file, this->prefetch_lookahead, this->prefetch_buffer_size);
        scheduler.schedule(&this->progress_bar);
        this->progress_bar.finish();
        this->stats.num_prefetch_alloc_failures = scheduler.get_num_allocation_failures();
        this->stats.num_synchronous_swapins = scheduler.get_num_synchronous_swapins();
        if (this->verbose) {
            std::cout << "Finished scheduling swaps: " << scheduler.get_num_allocation_failures() << " allocation failures, " << scheduler.get_num_synchronous_swapins() << " synchronous swapins" << std::endl;
        }
    }

    void DefaultPipeline::plan(Program<BinnedPlacer>** p, PlacementPlugin plugin, std::function<void()> program) {
        auto program_start = std::chrono::steady_clock::now();
        this->program(p, plugin, program, this->program_name + ".prog");
        auto program_end = std::chrono::steady_clock::now();
        this->stats.placement_duration = std::chrono::duration_cast<std::chrono::milliseconds>(program_end - program_start);

        auto replacement_start = std::chrono::steady_clock::now();
        this->allocate(this->program_name + ".prog", this->program_name + ".repprog");
        auto replacement_end = std::chrono::steady_clock::now();
        this->stats.replacement_duration = std::chrono::duration_cast<std::chrono::milliseconds>(replacement_end - replacement_start);

        auto scheduling_start = std::chrono::steady_clock::now();
        this->schedule(this->program_name + ".repprog", this->program_name + ".memprog");
        auto scheduling_end = std::chrono::steady_clock::now();
        this->stats.scheduling_duration = std::chrono::duration_cast<std::chrono::milliseconds>(scheduling_end - scheduling_start);
    }

    const DefaultPipelineStats& DefaultPipeline::get_stats() const {
        return this->stats;
    }
}
