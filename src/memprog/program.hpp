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
#include "memprog/addr.hpp"
#include "memprog/instruction.hpp"
#include "memprog/programfile.hpp"
#include "stream.hpp"

namespace mage::memprog {
    class Program : public VirtProgramFileWriter {
    public:
        Program(std::string filename, PageShift shift = 16);
        ~Program();

        VirtAddr new_instruction(OpCode op, BitWidth width, VirtAddr arg0 = invalid_vaddr, VirtAddr arg1 = invalid_vaddr, VirtAddr arg2 = invalid_vaddr, std::uint32_t constant = 0) {
            OpInfo info(op);

            VirtInstruction v;
            v.header.operation = op;
            v.header.width = width;

            bool fresh_page;
            v.header.output = this->allocate_virtual(info.single_bit_output() ? 1 : width, fresh_page);

            v.header.flags = fresh_page ? FlagOutputPageFirstUse : 0;

            switch (info.format()) {
            case InstructionFormat::NoArgs:
                break;
            case InstructionFormat::OneArg:
                v.one_arg.input1 = arg0;
                break;
            case InstructionFormat::TwoArgs:
                v.two_args.input1 = arg0;
                v.two_args.input2 = arg1;
                break;
            case InstructionFormat::ThreeArgs:
                v.three_args.input1 = arg0;
                v.three_args.input2 = arg1;
                v.three_args.input3 = arg2;
                break;
            case InstructionFormat::Constant:
                v.constant.constant = constant;
                break;
            default:
                std::abort();
            }

            this->append_instruction(v);

            return v.header.output;
        }

        static Program* set_current_working_program(Program* cwp);
        static Program* get_current_working_program();

    private:
        VirtAddr allocate_virtual(BitWidth width, bool& fresh_page) {
            VirtAddr addr;
            assert(width != 0);
            if (pg_num(this->next_free_address, this->page_shift) == pg_num(this->next_free_address + width - 1, this->page_shift)) {
                addr = this->next_free_address;
            } else {
                addr = pg_next(this->next_free_address, this->page_shift);
            }
            this->next_free_address = addr + width;
            fresh_page = (pg_offset(addr, this->page_shift) == 0);
            return addr;
        }
        VirtAddr next_free_address;
        PageShift page_shift;
        static Program* current_working_program;
    };
}

#endif
