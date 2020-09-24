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

#ifndef MAGE_MEMPROG_PROGRAM_HPP_
#define MAGE_MEMPROG_PROGRAM_HPP_

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include "addr.hpp"
#include "instruction.hpp"
#include "memprog/placement.hpp"
#include "opcode.hpp"
#include "programfile.hpp"

namespace mage::memprog {
    template <typename Placer>
    class Program : public VirtProgramFileWriter {
    public:
        Program(std::string filename, PageShift shift = 16) : VirtProgramFileWriter(filename, shift), placer(shift) {
        }

        ~Program() {
            this->set_page_count(this->placer.get_num_pages());
            if (Program<Placer>::current_working_program == this) {
                Program<Placer>::current_working_program = nullptr;
            }
        }

        Instruction& instruction() {
            return this->current;
        }

        VirtAddr commit_instruction(BitWidth output_width) {
            if (output_width != 0) {
                bool fresh_page;
                this->current.header.output = this->placer.allocate_virtual(output_width, fresh_page);
                if (fresh_page) {
                    this->current.header.flags |= FlagOutputPageFirstUse;
                }
            }
            this->append_instruction(this->current);
            return this->current.header.output;
        }

        void recycle(VirtAddr addr, BitWidth width) {
            this->placer.deallocate_virtual(addr, width);
        }

        void communication_barrier(WorkerID to) {
            Instruction instr;
            instr.header.operation = OpCode::NetworkFlush;
            instr.header.flags = 0;
            instr.control.data = to;
            this->append_instruction(instr);
        }

        static Program<Placer>* set_current_working_program(Program<Placer>* cwp) {
            Program<Placer>* old_cwp = Program<Placer>::current_working_program;
            Program<Placer>::current_working_program = cwp;
            return old_cwp;
        }

        static Program<Placer>* get_current_working_program() {
            return Program<Placer>::current_working_program;
        }

    private:
        Instruction current;
        Placer placer;
        static Program<Placer>* current_working_program;
    };

    template <typename Placer>
    Program<Placer>* Program<Placer>::current_working_program = nullptr;

    using DefaultProgram = Program<BinnedPlacer>;
}

#endif
