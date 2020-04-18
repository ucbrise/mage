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

#ifndef MAGE_DSL_PROGRAM_HPP_
#define MAGE_DSL_PROGRAM_HPP_

#include <cstdint>
#include <memory>
#include <vector>

namespace mage::dsl {
    using InstructionID = std::uint64_t;
    using BitWidth = std::uint16_t;
    using BitOffset = std::uint8_t;
    enum class OpCode : std::uint8_t {
        Undefined = 0,
        Input,
        PublicConstant,
        IntAdd,
        IntIncrement,
        IntSub,
        IntDecrement,
        IntLess,
        Equal,
        IsZero,
        NonZero,
        BitNOT,
        BitAND,
        BitOR,
        BitXOR,
        BitSelect,
        ValueSelect,
        Swap
    };

    const constexpr InstructionID invalid_instruction = UINT64_MAX;

    struct Instruction {
        InstructionID input1;
        InstructionID input2;
        InstructionID input3;
        OpCode operation;
        BitWidth width;
        union {
            std::uint32_t constant;
            struct {
                BitOffset offset1;
                BitOffset offset2;
                BitOffset offset3;
            };
        };
    };

    class Program {
    public:
        virtual ~Program();
        virtual InstructionID new_instruction(OpCode op, BitWidth width, std::uint32_t constant = 0) = 0;
        virtual InstructionID new_instruction(OpCode op, BitWidth width, InstructionID arg0, BitOffset offset0, InstructionID arg1 = invalid_instruction, BitOffset offset1 = 0, InstructionID arg2 = invalid_instruction, BitOffset offset2 = 0) = 0;
        virtual void mark_output(InstructionID v) = 0;
        virtual std::uint64_t num_instructions() = 0;

        static Program* set_current_working_program(Program* cwp);
        static Program* get_current_working_program();

    private:
        static Program* current_working_program;
    };

    class ProgramMemory : public Program {
    public:
        InstructionID new_instruction(OpCode op, BitWidth width, std::uint32_t constant = 0) override {
            Instruction& v = this->instructions.emplace_back();
            v.input1 = invalid_instruction;
            v.input2 = invalid_instruction;
            v.input3 = invalid_instruction;
            v.operation = op;
            v.width = width;
            v.constant = constant;
            return this->instructions.size();
        }

        InstructionID new_instruction(OpCode op, BitWidth width, InstructionID arg0, BitOffset offset0, InstructionID arg1 = invalid_instruction, BitOffset offset1 = 0, InstructionID arg2 = invalid_instruction, BitOffset offset2 = 0) override {
            Instruction& v = this->instructions.emplace_back();
            v.input1 = arg0;
            v.input2 = arg1;
            v.input3 = arg2;
            v.operation = op;
            v.width = width;
            v.offset1 = offset0;
            v.offset2 = offset1;
            v.offset3 = offset2;
            return this->instructions.size();
        }

        void mark_output(InstructionID v) override {
            this->outputs.push_back(v);
        }

        std::uint64_t num_instructions() override {
            return this->instructions.size();
        }

    private:
        std::vector<Instruction> instructions;
        std::vector<InstructionID> outputs;
    };
}

#endif
