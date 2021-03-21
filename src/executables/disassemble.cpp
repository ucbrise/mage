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

#include <cstdlib>
#include "instruction.hpp"
#include "programfile.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " file.memprog" << std::endl;
        return EXIT_FAILURE;
    }
    std::string filename(argv[1]);
    if (filename.ends_with(".memprog") || filename.ends_with(".repprog")) {
        mage::PhysProgramFileReader program(argv[1]);
        mage::InstructionNumber num_instructions = program.get_header().num_instructions;
        for (mage::InstructionNumber i = 0; i != num_instructions; i++) {
            mage::PackedPhysInstruction& phys = program.start_instruction();
            std::cout << phys << std::endl;
            program.finish_instruction(phys.size());
        }
    } else if (filename.ends_with(".prog")) {
        mage::VirtProgramFileReader program(argv[1]);
        mage::InstructionNumber num_instructions = program.get_header().num_instructions;
        for (mage::InstructionNumber i = 0; i != num_instructions; i++) {
            mage::PackedVirtInstruction& virt = program.start_instruction();
            std::cout << virt << std::endl;
            program.finish_instruction(virt.size());
        }
    } else {
        std::cout << "Error: could not infer bytecode type from file extension" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
