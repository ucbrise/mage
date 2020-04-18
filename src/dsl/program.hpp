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

#include "stream.hpp"
#include <cstdint>
#include <memory>
#include <vector>

namespace mage::dsl {
    using Address = std::uint64_t;
    using BitWidth = std::uint8_t;
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

    const constexpr int address_bits = 52;
    const constexpr Address invalid_addr = (UINT64_C(1) << address_bits) - 1;

    struct Instruction {
        Address input1 : address_bits;
        Address input2 : address_bits;
        Address input3 : address_bits;
        Address output : address_bits;
        OpCode operation;
        BitWidth width;
        std::uint32_t constant;
    } __attribute__((packed));

    class Program {
    public:
        Program();
        virtual ~Program();

        Address new_instruction(OpCode op, BitWidth width, Address arg0 = invalid_addr, Address arg1 = invalid_addr, Address arg2 = invalid_addr, std::uint32_t constant = 0) {
            Instruction v;
            v.input1 = arg0;
            v.input2 = arg1;
            v.input3 = arg2;
            v.output = this->next_free_address;
            v.operation = op;
            v.width = width;
            v.constant = constant;
            this->next_free_address += width;
            this->append_instruction(v);
            return v.output;
        }

        virtual void mark_output(Address v, BitWidth length) = 0;
        virtual std::uint64_t num_instructions() = 0;

        static Program* set_current_working_program(Program* cwp);
        static Program* get_current_working_program();

    protected:
        virtual void append_instruction(const Instruction& v) = 0;
        Address next_free_address;

    private:
        static Program* current_working_program;
    };

    // class ProgramMemory : public Program {
    // public:
    //     void mark_output(Address v, BitWidth length) override {
    //         this->outputs.push_back(v);
    //     }
    //
    //     std::uint64_t num_instructions() override {
    //         return this->instructions.size();
    //     }
    //
    // protected:
    //     void append_instruction(const Instruction& v) override {
    //         this->instructions.push_back(v);
    //     }
    //
    // private:
    //     std::vector<Instruction> instructions;
    //     std::vector<Address> outputs;
    // };
}

#endif
