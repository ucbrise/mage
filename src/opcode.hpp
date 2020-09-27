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

#ifndef MAGE_OPCODE_HPP_
#define MAGE_OPCODE_HPP_

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include "addr.hpp"

namespace mage {
    enum class OpCode : std::uint8_t {
        Undefined = 0,
        IssueSwapIn,
        IssueSwapOut,
        FinishSwapIn,
        FinishSwapOut,
        CopySwap,
        NetworkPostReceive,
        NetworkFinishReceive,
        NetworkBufferSend,
        NetworkFinishSend,
        Input, // 1 argument
        Output, // 1 argument
        PublicConstant, // 0 arguments
        Copy, // 1 argument
        IntAdd, // 2 arguments
        IntIncrement, // 1 argument
        IntSub, // 2 arguments
        IntDecrement, // 1 argument
        IntLess, // 2 arguments
        Equal, // 2 arguments
        IsZero, // 1 argument
        NonZero, // 1 argument
        BitNOT, // 1 argument
        BitAND, // 2 arguments
        BitOR, // 2 arguments
        BitXOR, // 2 arguments
        ValueSelect // 3 arguments
    };

    constexpr const char* opcode_to_string(OpCode op) {
        switch (op) {
        case OpCode::Undefined:
            return "Undefined";
        case OpCode::IssueSwapIn:
            return "IssueSwapIn";
        case OpCode::IssueSwapOut:
            return "IssueSwapOut";
        case OpCode::FinishSwapIn:
            return "FinishSwapIn";
        case OpCode::FinishSwapOut:
            return "FinishSwapOut";
        case OpCode::CopySwap:
            return "CopySwap";
        case OpCode::NetworkPostReceive:
            return "NetworkPostReceive";
        case OpCode::NetworkFinishReceive:
            return "NetworkFinishReceive";
        case OpCode::NetworkBufferSend:
            return "NetworkBufferSend";
        case OpCode::NetworkFinishSend:
            return "NetworkFinishSend";
        case OpCode::Input:
            return "Input";
        case OpCode::Output:
            return "Output";
        case OpCode::PublicConstant:
            return "PublicConstant";
        case OpCode::Copy:
            return "Copy";
        case OpCode::IntAdd:
            return "IntAdd";
        case OpCode::IntIncrement:
            return "IntIncrement";
        case OpCode::IntSub:
            return "IntSub";
        case OpCode::IntDecrement:
            return "IntDecrement";
        case OpCode::IntLess:
            return "IntLess";
        case OpCode::Equal:
            return "Equal";
        case OpCode::IsZero:
            return "IsZero";
        case OpCode::NonZero:
            return "NonZero";
        case OpCode::BitNOT:
            return "BitNOT";
        case OpCode::BitAND:
            return "BitAND";
        case OpCode::BitOR:
            return "BitOR";
        case OpCode::BitXOR:
            return "BitXOR";
        case OpCode::ValueSelect:
            return "ValueSelect";
        default:
            std::abort();
        }
    }

    enum class InstructionFormat : std::uint8_t {
        NoArgs = 0,
        OneArg = 1,
        TwoArgs = 2,
        ThreeArgs = 3,
        Constant = 4,
        Swap = 5,
        SwapFinish = 6,
        Control = 7
    };

    constexpr int instruction_format_num_args(InstructionFormat format) {
        switch (format) {
        case InstructionFormat::NoArgs:
            return 0;
        case InstructionFormat::OneArg:
            return 1;
        case InstructionFormat::TwoArgs:
            return 2;
        case InstructionFormat::ThreeArgs:
            return 3;
        case InstructionFormat::Constant:
        case InstructionFormat::Swap:
        case InstructionFormat::SwapFinish:
        case InstructionFormat::Control:
            return 0;
        default:
            std::abort();
        }
    }

    constexpr bool instruction_format_uses_constant(InstructionFormat format) {
        switch (format) {
        case InstructionFormat::NoArgs:
        case InstructionFormat::OneArg:
        case InstructionFormat::TwoArgs:
        case InstructionFormat::ThreeArgs:
            return false;
        case InstructionFormat::Constant:
            return true;
        case InstructionFormat::Swap:
        case InstructionFormat::SwapFinish:
        case InstructionFormat::Control:
            return false;
        default:
            std::abort();
        }
    }

    class OpInfo {
    public:
        constexpr OpInfo(OpCode op) : layout(InstructionFormat::NoArgs), single_bit(false), has_output(true) {
            this->set(op);
        }

        constexpr OpInfo& operator=(OpCode op) {
            this->set(op);
            return *this;
        }

        constexpr void set(OpCode op)  {
            switch (op) {
            case OpCode::Input:
                this->layout = InstructionFormat::NoArgs;
                this->single_bit = false;
                this->has_output = true;
                break;
            case OpCode::Output:
                this->layout = InstructionFormat::NoArgs;
                this->single_bit = false;
                this->has_output = false;
                break;
            case OpCode::IssueSwapIn:
                this->layout = InstructionFormat::Swap;
                this->single_bit = false;
                this->has_output = true;
                break;
            case OpCode::IssueSwapOut:
            case OpCode::CopySwap:
                this->layout = InstructionFormat::Swap;
                this->single_bit = false;
                this->has_output = false;
                break;
            case OpCode::FinishSwapIn:
            case OpCode::FinishSwapOut:
                this->layout = InstructionFormat::SwapFinish;
                this->single_bit = false;
                this->has_output = false;
                break;
            case OpCode::NetworkPostReceive:
                this->layout = InstructionFormat::Constant;
                this->single_bit = false;
                this->has_output = true;
                break;
            case OpCode::NetworkFinishReceive:
                this->layout = InstructionFormat::Control;
                this->single_bit = false;
                this->has_output = false;
                break;
            case OpCode::NetworkBufferSend:
                this->layout = InstructionFormat::Constant;
                this->single_bit = false;
                this->has_output = false;
                break;
            case OpCode::NetworkFinishSend:
                this->layout = InstructionFormat::Control;
                this->single_bit = false;
                this->has_output = false;
                break;
            case OpCode::PublicConstant:
                this->layout = InstructionFormat::Constant;
                this->single_bit = false;
                this->has_output = true;
                break;
            case OpCode::Copy:
                this->layout = InstructionFormat::OneArg;
                this->single_bit = false;
                this->has_output = true;
                break;
            case OpCode::IntAdd:
            case OpCode::IntSub:
            case OpCode::BitAND:
            case OpCode::BitOR:
            case OpCode::BitXOR:
                this->layout = InstructionFormat::TwoArgs;
                this->single_bit = false;
                this->has_output = true;
                break;
            case OpCode::IntIncrement:
            case OpCode::IntDecrement:
            case OpCode::BitNOT:
                this->layout = InstructionFormat::OneArg;
                this->single_bit = false;
                this->has_output = true;
                break;
            case OpCode::IntLess:
            case OpCode::Equal:
                this->layout = InstructionFormat::TwoArgs;
                this->single_bit = true;
                this->has_output = true;
                break;
            case OpCode::IsZero:
            case OpCode::NonZero:
                this->layout = InstructionFormat::OneArg;
                this->single_bit = true;
                this->has_output = true;
                break;
            case OpCode::ValueSelect:
                this->layout = InstructionFormat::ThreeArgs;
                this->single_bit = false;
                this->has_output = true;
                break;
            default:
                std::abort();
            }
        }

        constexpr int num_args() const {
            return instruction_format_num_args(this->layout);
        }

        constexpr bool uses_constant() const {
            return instruction_format_uses_constant(this->layout);
        }

        constexpr bool single_bit_output() const {
            return this->single_bit;
        }

        constexpr bool has_variable_output() const {
            return this->has_output;
        }

        constexpr InstructionFormat format() const {
            return this->layout;
        }

    private:
        InstructionFormat layout;
        bool single_bit;
        bool has_output;
    };
}

#endif
