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

#ifndef MAGE_MEMPROG_PIPELINE_HPP_
#define MAGE_MEMPROG_PIPELINE_HPP_

#include <cstdint>
#include <chrono>
#include <functional>
#include <string>
#include "memprog/annotation.hpp"
#include "memprog/placement.hpp"
#include "memprog/replacement.hpp"
#include "util/config.hpp"

namespace mage::memprog {
    template <typename Placer>
    class Pipeline {
    public:
        Pipeline(const std::string& name) : program_name(name) {
        }

        Pipeline(const std::string& name, const util::ConfigValue& worker) : program_name(name) {
            this->read_config(worker);
        }

        void set_program_name(const std::string& name) {
            this->program_name = name;
        }

        virtual void set_verbose(bool be_verbose) = 0;
        virtual void read_config(const util::ConfigValue& worker) = 0;
        virtual void plan(Program<Placer>** p, PlacementPlugin plugin, std::function<void()> program) = 0;

    protected:
        std::string program_name;
    };

    struct DefaultPipelineStats {
        InstructionNumber num_instructions;
        std::uint64_t num_swapouts;
        std::uint64_t num_swapins;
        StoragePageNumber num_storage_frames;
        std::uint64_t num_prefetch_alloc_failures;
        std::uint64_t num_synchronous_swapins;

        std::chrono::milliseconds placement_duration;
        std::chrono::milliseconds replacement_duration;
        std::chrono::milliseconds scheduling_duration;
    };

    class DefaultPipeline : public Pipeline<BinnedPlacer> {
    public:
        DefaultPipeline(const std::string& name);
        DefaultPipeline(const std::string& name, const util::ConfigValue& worker);

        void set_verbose(bool be_verbose) override;

        void read_config(const util::ConfigValue& worker) override;

        virtual void program(Program<BinnedPlacer>** p, PlacementPlugin plugin, std::function<void()> dsl_program, const std::string& prog_file);
        virtual void allocate(const std::string& prog_file, const std::string& repprog_file);
        virtual void schedule(const std::string& repprog_file, const std::string& memprog_file);

        void plan(Program<BinnedPlacer>** p, PlacementPlugin plugin, std::function<void()> program) override;

        const DefaultPipelineStats& get_stats() const;

    private:
        PageShift page_shift;
        VirtPageNumber num_pages;
        VirtPageNumber prefetch_buffer_size;
        InstructionNumber prefetch_lookahead;

        DefaultPipelineStats stats;
        bool verbose;
    };
}

#endif
