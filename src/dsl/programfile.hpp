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

#ifndef MAGE_DSL_PROGRAMFILE_HPP_
#define MAGE_DSL_PROGRAMFILE_HPP_

#include "dsl/program.hpp"

#include <cstdint>

#include <iostream>
#include <fstream>
#include <string>

namespace mage::dsl {
    struct OutputRange {
        VirtAddr start;
        VirtAddr end;
    };
    
    struct ProgramFileHeader {
        InstructionNumber num_instructions;
        std::uint64_t num_output_ranges;
        VirtualInstruction instructions[0];

        OutputRange* get_output_ranges() {
            return reinterpret_cast<OutputRange*>(&this->instructions[this->num_instructions]);
        }
    };

    class ProgramFileWriter : public Program {
    public:
        ProgramFileWriter(std::string filename);
        ~ProgramFileWriter();

        void mark_output(VirtAddr v, BitWidth length) override;
        VirtAddr num_instructions() override;

    protected:
        void append_instruction(const VirtualInstruction& v) override;

    private:
        std::uint64_t count;
        std::vector<OutputRange> outputs;
        std::ofstream output;
    };

    class ProgramFileReader {
    public:
        ProgramFileReader(std::string filename);

        std::uint64_t read_next_instruction(VirtualInstruction& instruction);
        const std::vector<OutputRange>& get_outputs() const;

    private:
        ProgramFileHeader header;
        std::uint64_t next_instruction;
        std::ifstream input;
        std::vector<OutputRange> outputs;
    };
}

#endif
