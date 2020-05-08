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

#ifndef MAGE_MEMPROG_PROGRAMFILE_HPP_
#define MAGE_MEMPROG_PROGRAMFILE_HPP_

#include <cstdint>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "memprog/addr.hpp"
#include "memprog/instruction.hpp"

namespace mage::memprog {
    struct OutputRange {
        std::uint64_t start;
        std::uint64_t end;
    };

    struct ProgramFileHeader {
        InstructionNumber num_instructions;
        std::uint64_t num_output_ranges;
        std::uint64_t ranges_offset;

        OutputRange* get_output_ranges() {
            return reinterpret_cast<OutputRange*>(reinterpret_cast<char*>(this) + this->ranges_offset);
        }
    };

    template <std::uint8_t addr_bits>
    class ProgramFileWriter {
    public:
        ProgramFileWriter(std::string filename) : count(0) {
            this->output.exceptions(std::ios::failbit | std::ios::badbit);
            this->output.open(filename, std::ios::out | std::ios::binary | std::ios::trunc);

            ProgramFileHeader header = { 0 };
            this->output.write(reinterpret_cast<const char*>(&header), sizeof(header));
        }

        virtual ~ProgramFileWriter() {
            std::streampos ranges_offset = this->output.tellp();
            this->output.write(reinterpret_cast<const char*>(outputs.data()), outputs.size() * sizeof(OutputRange));
            this->output.seekp(0, std::ios::beg);

            ProgramFileHeader header;
            header.num_instructions = this->count;
            header.num_output_ranges = this->outputs.size();
            header.ranges_offset = ranges_offset;
            this->output.write(reinterpret_cast<const char*>(&header), sizeof(header));
        }

        void mark_output(std::uint8_t v, BitWidth length) {
            if (this->outputs.size() != 0 && this->outputs.back().end == v) {
                this->outputs.back().end = v + length;
            } else {
                OutputRange& r = this->outputs.emplace_back();
                r.start = v;
                r.end = v + length;
            }
        }

        std::uint64_t num_instructions() const {
            return this->count;
        }

        void append_instruction(const Instruction<addr_bits>& v) {
            v.write_to_output(this->output);
            this->count++;
        }

    private:
        std::uint64_t count;
        std::vector<OutputRange> outputs;
        std::ofstream output;
    };

    class ProgramFileReader {
    public:
        ProgramFileReader(std::string filename);

        std::uint64_t read_next_instruction(VirtInstruction& instruction);
        const std::vector<OutputRange>& get_outputs() const;

    private:
        ProgramFileHeader header;
        std::uint64_t next_instruction;
        std::ifstream input;
        std::vector<OutputRange> outputs;
    };

    using VirtProgramFileWriter = ProgramFileWriter<virtual_address_bits>;
}

#endif
