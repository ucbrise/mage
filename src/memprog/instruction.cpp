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

#include "memprog/instruction.hpp"
#include <cstdlib>

namespace mage::memprog {
    void OpInfo::set(OpCode op) {
        switch (op) {
        case OpCode::Input:
            this->layout = InstructionFormat::NoArgs;
            this->single_bit = false;
            break;
        case OpCode::PublicConstant:
            this->layout = InstructionFormat::Constant;
            this->single_bit = false;
            break;
        case OpCode::IntAdd:
        case OpCode::IntSub:
        case OpCode::BitAND:
        case OpCode::BitOR:
        case OpCode::BitXOR:
            this->layout = InstructionFormat::TwoArgs;
            this->single_bit = false;
            break;
        case OpCode::IntIncrement:
        case OpCode::IntDecrement:
        case OpCode::BitNOT:
            this->layout = InstructionFormat::OneArg;
            this->single_bit = false;
            break;
        case OpCode::IntLess:
        case OpCode::Equal:
            this->layout = InstructionFormat::TwoArgs;
            this->single_bit = true;
            break;
        case OpCode::IsZero:
        case OpCode::NonZero:
            this->layout = InstructionFormat::OneArg;
            this->single_bit = true;
            break;
        case OpCode::ValueSelect:
            this->layout = InstructionFormat::ThreeArgs;
            this->single_bit = false;
            break;
        default:
            std::abort();
        }
    }

    int OpInfo::num_args() const {
        switch (this->layout) {
        case InstructionFormat::NoArgs:
            return 0;
        case InstructionFormat::OneArg:
            return 1;
        case InstructionFormat::TwoArgs:
            return 2;
        case InstructionFormat::ThreeArgs:
            return 3;
        case InstructionFormat::Constant:
            return 0;
        default:
            std::abort();
        }
    }

    bool OpInfo::uses_constant() const {
        switch (this->layout) {
        case InstructionFormat::NoArgs:
        case InstructionFormat::OneArg:
        case InstructionFormat::TwoArgs:
        case InstructionFormat::ThreeArgs:
            return false;
        case InstructionFormat::Constant:
            return true;
        default:
            std::abort();
        }
    }

    bool OpInfo::single_bit_output() const {
        return this->single_bit;
    }
}
