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

#include "memprog/scheduling.hpp"
#include <string>
#include <cstdint>
#include <cstdlib>
#include "addr.hpp"
#include "instruction.hpp"
#include "opcode.hpp"
#include "programfile.hpp"

namespace mage::memprog {
    Scheduler::Scheduler(std::string input_file, std::string output_file)
        : input(input_file), output(output_file) {
    }

    Scheduler::~Scheduler() {
    }

    void Scheduler::emit_finish_swapin(PhysPageNumber ppn) {
        constexpr std::size_t length = PackedPhysInstruction::size(InstructionFormat::Nothing);

        PackedPhysInstruction& finish = this->output.start_instruction();
        finish.header.operation = OpCode::FinishSwapIn;
        finish.header.flags = 0;
        finish.header.output = ppn;
        this->output.finish_instruction(length);
    }

    void Scheduler::emit_finish_swapout(PhysPageNumber ppn) {
        constexpr std::size_t length = PackedPhysInstruction::size(InstructionFormat::Nothing);

        PackedPhysInstruction& finish = this->output.start_instruction();
        finish.header.operation = OpCode::FinishSwapOut;
        finish.header.flags = 0;
        finish.header.output = ppn;
        this->output.finish_instruction(length);
    }

    NOPScheduler::NOPScheduler(std::string input_file, std::string output_file)
        : Scheduler(input_file, output_file) {
        const ProgramFileHeader& header = this->input.get_header();
        this->output.set_page_count(header.num_pages);
        this->output.set_swap_page_count(header.num_pages);
        this->output.set_page_shift(header.page_shift);
    }

    void NOPScheduler::schedule() {
        const ProgramFileHeader& header = this->input.get_header();
        for (std::uint64_t i = 0; i != header.num_instructions; i++) {
            const PackedPhysInstruction& phys = this->input.start_instruction();
            const std::uint8_t* phys_start = reinterpret_cast<const std::uint8_t*>(&phys);
            std::size_t phys_size = phys.size();

            PackedPhysInstruction& into = this->output.start_instruction();
            std::copy(phys_start, phys_start + phys_size, reinterpret_cast<std::uint8_t*>(&into));
            this->output.finish_instruction(phys_size);

            if (phys.header.operation == OpCode::IssueSwapIn) {
                this->emit_finish_swapin(phys.header.output);
            } else if (phys.header.operation == OpCode::IssueSwapOut) {
                this->emit_finish_swapout(phys.header.output);
            }

            this->input.finish_instruction(phys_size);
        }
    }
}
