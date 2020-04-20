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
    using InstructionNumber = std::uint64_t;
    const constexpr int instruction_number_bits = 48;
    const constexpr std::uint64_t invalid_instr = (UINT64_C(1) << instruction_number_bits) - 1;

    using VirtAddr = std::uint64_t;
    const constexpr int virtual_address_bits = 52;
    const constexpr VirtAddr invalid_vaddr = (UINT64_C(1) << virtual_address_bits) - 1;

    using BitWidth = std::uint8_t;

    enum class OpCode : std::uint8_t {
        Undefined = 0,
        SwapIn,
        SwapOut,
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

    struct VirtualInstruction {
        VirtAddr input1 : virtual_address_bits;
        VirtAddr input2 : virtual_address_bits;
        VirtAddr input3 : virtual_address_bits;
        VirtAddr output : virtual_address_bits;
        OpCode operation;
        BitWidth width;
        std::uint32_t constant;
    } __attribute__((packed));

    class Program {
    public:
        Program();
        virtual ~Program();

        VirtAddr new_instruction(OpCode op, BitWidth width, VirtAddr arg0 = invalid_vaddr, VirtAddr arg1 = invalid_vaddr, VirtAddr arg2 = invalid_vaddr, std::uint32_t constant = 0) {
            VirtualInstruction v;
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

        virtual void mark_output(VirtAddr v, BitWidth length) = 0;
        virtual std::uint64_t num_instructions() = 0;

        static Program* set_current_working_program(Program* cwp);
        static Program* get_current_working_program();

    protected:
        virtual void append_instruction(const VirtualInstruction& v) = 0;
        VirtAddr next_free_address;

    private:
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
