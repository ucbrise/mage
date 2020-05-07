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

#include "stream.hpp"
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include "memprog/addr.hpp"
#include "memprog/instruction.hpp"

namespace mage::memprog {
    using InstructionNumber = std::uint64_t;
    const constexpr int instruction_number_bits = 48;
    const constexpr std::uint64_t invalid_instr = (UINT64_C(1) << instruction_number_bits) - 1;

    class Program {
    public:
        Program(PageShift shift);
        virtual ~Program();

        VirtAddr new_instruction(OpCode op, BitWidth width, VirtAddr arg0 = invalid_vaddr, VirtAddr arg1 = invalid_vaddr, VirtAddr arg2 = invalid_vaddr, std::uint32_t constant = 0) {
            OpInfo info(op);
            VirtInstruction v;
            v.header.operation = op;
            v.header.width = width;
            v.header.constant_mask = 0;
            v.header.output = this->allocate_virtual(info.single_bit_output() ? 1 : width);

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

        virtual void mark_output(VirtAddr v, BitWidth length) = 0;
        virtual std::uint64_t num_instructions() = 0;

        static Program* set_current_working_program(Program* cwp);
        static Program* get_current_working_program();

    protected:
        virtual void append_instruction(const VirtInstruction& v) = 0;
        VirtAddr next_free_address;
        PageShift page_shift;

    private:
        VirtAddr allocate_virtual(BitWidth width) {
            VirtAddr addr;
            if (pgnum(this->next_free_address, this->page_shift) == pgnum(this->next_free_address + width, this->page_shift)) {
                addr = this->next_free_address;
            } else {
                addr = pg_round_up(this->next_free_address, this->page_shift);
            }
            this->next_free_address = addr + width;
            return addr;
        }
        static Program* current_working_program;
    };

    // class ProgramMemory : public Program {
    // public:
    //     void mark_output(VirtAddr v, BitWidth length) override {
    //         this->outputs.push_back(v);
    //     }
    //
    //     std::uint64_t num_instructions() override {
    //         return this->instructions.size();
    //     }
    //
    // protected:
    //     void append_instruction(const VirtualInstruction& v) override {
    //         this->instructions.push_back(v);
    //     }
    //
    // private:
    //     std::vector<VirtualInstruction> instructions;
    //     std::vector<VirtAddr> outputs;
    // };
}

#endif
