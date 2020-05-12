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

#include <string>
#include "addr.hpp"
#include "engine/engine.hpp"
#include "programfile.hpp"

namespace mage::engine {
    template <typename Protocol>
    class SingleCoreEngine : private Engine<Protocol> {
    public:
        SingleCoreEngine(std::string program, Protocol& prot) : Engine<Protocol>(prot), input(program.c_str()) {
            const ProgramFileHeader& header = this->input.get_header();
            this->init(header.page_shift, header.num_pages);
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
