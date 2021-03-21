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

/**
 * @file memprog/pipeline.hpp
 * @brief Overall memory programming pipeline for MAGE's planner.
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
#include "util/progress.hpp"

namespace mage::memprog {
    /**
     * @brief An abstract class for a pipeline for generating a memory program.
     *
     * @tparam Placer The type of the placement module to use.
     */
    template <typename Placer>
    class Pipeline {
    public:
        /**
         * @brief Creates a pipeline to create a memory program with the given
         * name.
         *
         * @param name The name of the memory program to create.
         */
        Pipeline(const std::string& name) : program_name(name) {
        }

        /**
         * @brief Creates a pipeline to create a memory program with the given
         * name and configuration.
         *
         * Subclass functionality is invoked to interpret the configuration.
         *
         * @param name The name of the memory program to create.
         * @param worker The node in the configuration file corresponding to
         * the worker whose memory program to generate.
         */
        Pipeline(const std::string& name, const util::ConfigValue& worker) : program_name(name) {
            this->read_config(worker);
        }

        /**
         * @brief Sets the name of the memory program to generate.
         *
         * @param name The name of the memory program to generate.
         */
        void set_program_name(const std::string& name) {
            this->program_name = name;
        }

        /**
         * @brief Sets the verbosity of memory program generation.
         *
         * @param be_verbose If true, status information is printed out after
         * each stage of the pipeline; if false, nothing is printed out.
         */
        virtual void set_verbose(bool be_verbose) = 0;

        /**
         * @brief Sets configuration information for the worker for whom the
         * memory program is being generated,
         *
         * @param worker The node in the configuration file corresponding to
         * the worker whose memory program to generate.
         */
        virtual void read_config(const util::ConfigValue& worker) = 0;

        /**
         * @brief Runs the pipeline and generates a memory program.
         *
         * @param p Pointer to the program object pointer used by the DSL in
         * which the program is written.
         * @param plugin Plugin for the target protocol, used for placement of
         * data in the MAGE-virtual address space.
         * @param program The program whose execution to plan.
         */
        virtual void plan(Program<Placer>** p, PlacementPlugin plugin, std::function<void()> program) = 0;

    protected:
        std::string program_name;
    };

    /**
     * @brief Contains statistics collected by MAGE's planner.
     *
     * This structure is meant to be populated by a @p DefaultPipeline as it
     * runs.
     */
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

    /**
     * @brief The memory programming pipeline described in the OSDI 2021 paper
     * describing MAGE.
     */
    class DefaultPipeline : public Pipeline<BinnedPlacer> {
    public:
        /**
         * @brief Creates an instance of the default pipeline to create a
         * memory program with the given name.
         *
         * @param name The name of the memory program to create.
         */
        DefaultPipeline(const std::string& name);

        /**
         * @brief Creates an instance of the default pipeline to create a
         * memory program with the given name and configuration.
         *
         * @param name The name of the memory program to create.
         * @param worker The node in the configuration file corresponding to
         * the worker whose memory program to generate.
         */
        DefaultPipeline(const std::string& name, const util::ConfigValue& worker);

        void set_verbose(bool be_verbose) override;

        void read_config(const util::ConfigValue& worker) override;

        /**
         * @brief Runs the "Placement" stage of the planning pipeline. Invoked
         * by the @p plan function.
         *
         * @param p Pointer to the program object pointer used by the DSL in
         * which the program is written.
         * @param plugin Plugin for the target protocol, used for placement of
         * data in the MAGE-virtual address space.
         * @param dsl_program The program whose execution to plan.
         * @param prog_file The name of the file to which to write the output
         * of the "Placement" stage (virtual bytecode).
         */
        virtual void program(Program<BinnedPlacer>** p, PlacementPlugin plugin, std::function<void()> dsl_program, const std::string& prog_file);

        /**
         * @brief Runs the "Replacement" stage of the planning pipeline,
         * including the preceding reverse pass to annotate the proram. Invoked
         * by the @p plan function.
         *
         * This program creates a temporary file to which it writes
         * the annotations during the reverse pass.
         *
         * @param prog_file The name of the file containing the virtual
         * bytecode (output of the "Placement" stage), which is read as input
         * in this stage.
         * @param repprog_file The name of the file to which to write the
         * output of the "Replacement" stage (physical bytecode).
         */
        virtual void allocate(const std::string& prog_file, const std::string& repprog_file);

        /**
         * @brief Runs the "Scheduling" stage of the planning pipeline. Invoked
         * by the @p plan function.
         *
         *
         * @param repprog_file The name of the file containing the physical
         * bytecode (output of the "Replacement" stage), which is read as input
         * in this stage.
         * @param memprog_file The name of the file to which to write the
         * output of the "Scheduling" stage (the final memory program).
         */
        virtual void schedule(const std::string& repprog_file, const std::string& memprog_file);

        void plan(Program<BinnedPlacer>** p, PlacementPlugin plugin, std::function<void()> program) override;

        /**
         * @brief Returns statistics collected during the planning phase.
         *
         * @return A reference to a structure containing statistics collected
         * when running the planning pipeline.
         */
        const DefaultPipelineStats& get_stats() const;

    private:
        PageShift page_shift;
        VirtPageNumber num_pages;
        VirtPageNumber prefetch_buffer_size;
        InstructionNumber prefetch_lookahead;

        DefaultPipelineStats stats;
        util::ProgressBar progress_bar;
        bool verbose;
    };
}

#endif
