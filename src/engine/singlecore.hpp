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

#ifndef MAGE_ENGINE_SINGLECORE_HPP_
#define MAGE_ENGINE_SINGLECORE_HPP_

#include <iostream>
#include <memory>
#include <string>
#include "addr.hpp"
#include "engine/engine.hpp"
#include "programfile.hpp"
#include "util/resourceset.hpp"
#include "util/stats.hpp"

namespace mage::engine {
    template <typename ProtEngine>
    class SingleCoreEngine : private Engine<ProtEngine> {
    public:
        SingleCoreEngine(std::shared_ptr<ClusterNetwork>& network, const util::ResourceSet::Worker& worker, ProtEngine& prot, std::string program)
            : Engine<ProtEngine>(network, prot), input(program.c_str()) {
            const ProgramFileHeader& header = this->input.get_header();
            this->init(worker, header.page_shift, header.num_pages, header.num_swap_pages, header.max_concurrent_swaps);
            this->input.enable_stats("READ-INSTR (ns)");
        }

        void execute_program() {
            InstructionNumber num_instructions = this->input.get_header().num_instructions;
            for (InstructionNumber i = 0; i != num_instructions; i++) {
                PackedPhysInstruction& phys = this->input.start_instruction();
                std::size_t size = this->execute_instruction(phys);
                this->input.finish_instruction(size);
            }
        }

    private:
        PhysProgramFileReader input;
    };
}

#endif
