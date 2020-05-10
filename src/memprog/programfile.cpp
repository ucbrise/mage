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

#include "memprog/programfile.hpp"

#include <iostream>
#include <fstream>
#include <string>

namespace mage::memprog {
    ProgramFileReader::ProgramFileReader(std::string filename) {
        this->input.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
        this->input.open(filename, std::ios::in | std::ios::binary);
        this->input.read(reinterpret_cast<char*>(&this->header), sizeof(this->header));
        this->input.seekg(this->header.num_instructions * sizeof(Instruction), std::ios::cur);
        this->outputs.resize(this->header.num_output_ranges);
        this->input.read(reinterpret_cast<char*>(this->outputs.data()), this->header.num_output_ranges * sizeof(OutputRange));
        this->input.seekg(sizeof(this->header), std::ios::beg);
    }

    std::uint64_t ProgramFileReader::read_next_instruction(Instruction& instruction) {
        if (this->next_instruction == this->header.num_instructions) {
            return invalid_instr;
        }
        instruction.read_from_input<physical_address_bits, storage_address_bits>(this->input);
        return this->next_instruction++;
    }

    const std::vector<OutputRange>& ProgramFileReader::get_outputs() const {
        return this->outputs;
    }
}
