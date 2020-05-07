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

#ifndef MAGE_MEMPROG_OPCODE_HPP_
#define MAGE_MEMPROG_OPCODE_HPP_

#include <cstdint>
#include "util/binary.hpp"

namespace mage::memprog {
    using BitWidth = std::uint8_t;
    enum class OpCode : std::uint8_t {
        Undefined = 0,
        SwapIn,
        SwapOut,
        Input, // 1 argument
        PublicConstant, // 0 arguments
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

    enum class InstructionFormat : std::uint8_t {
        NoArgs = 0,
        OneArg = 1,
        TwoArgs = 2,
        ThreeArgs = 3,
        Constant = 4
    };

    class OpInfo {
    public:
        OpInfo(OpCode op) {
            this->set(op);
        }

        OpInfo& operator=(OpCode op) {
            this->set(op);
            return *this;
        }

        void set(OpCode op);

        int num_args() const;
        bool uses_constant() const;
        bool single_bit_output() const;

        InstructionFormat format() const {
            return this->layout;
        }

    private:
        InstructionFormat layout;
        bool single_bit;
    };

    template <std::uint8_t addr_bits>
    struct PackedInstruction {
        static const constexpr std::uint8_t addr_bytes = addr_bits >> 3;
        static_assert((addr_bits & 0x7) == 0, "Address width not a whole number of bytes");

        struct {
            OpCode operation;
            BitWidth width;
            std::uint8_t constant_mask;
            std::uint64_t output : addr_bits;
        } __attribute__((packed)) header;
        union {
            struct {
                std::uint8_t next[0];
            } __attribute__((packed)) no_args;
            struct {
                std::uint64_t constant;
                std::uint8_t next[0];
            } __attribute__((packed)) constant;
            struct {
                std::uint64_t input1 : addr_bits;
                std::uint8_t next[0];
            } __attribute__((packed)) one_arg;
            struct {
                std::uint64_t input1 : addr_bits;
                std::uint64_t input2 : addr_bits;
                std::uint8_t next[0];
            } __attribute__((packed)) two_args;
            struct {
                std::uint64_t input1 : addr_bits;
                std::uint64_t input2 : addr_bits;
                std::uint64_t input3 : addr_bits;
                std::uint8_t next[0];
            } __attribute__((packed)) three_args;
        };

        /* Useful if we memory-map a program. */
        PackedInstruction<addr_bits>* next() const {
            OpInfo info(this->op);
            std::uint8_t* rv;
            switch (info.format()) {
            case InstructionFormat::NoArgs:
                rv = &this->no_args.next[0];
                break;
            case InstructionFormat::OneArg:
                rv = &this->one_arg.next[0];
                break;
            case InstructionFormat::TwoArgs:
                rv = &this->two_args.next[0];
                break;
            case InstructionFormat::ThreeArgs:
                rv = &this->three_args.next[0];
                break;
            case InstructionFormat::Constant:
                rv = &this->constant.next[0];
                break;
            default:
                std::abort();
            }
            return reinterpret_cast<PackedInstruction<addr_bits>*>(rv);
        }
    } __attribute__((packed));

    template <std::uint8_t addr_bits>
    struct Instruction {
        static const constexpr std::uint8_t addr_bytes = addr_bits >> 3;
        static_assert((addr_bits & 0x7) == 0, "Address width not a whole number of bytes");

        struct {
            OpCode operation;
            BitWidth width;
            std::uint8_t constant_mask;
            std::uint64_t output;
        } header;

        union {
            struct {
                // Input
            } no_args;
            struct {
                std::uint64_t constant;
            } constant;
            struct {
                std::uint64_t input1;
            } one_arg;
            struct {
                std::uint64_t input1;
                std::uint64_t input2;
            } two_args;
            struct {
                std::uint64_t input1;
                std::uint64_t input2;
                std::uint64_t input3;
            } three_args;
        };

        void write_to_output(std::ostream& out) const {
            OpInfo info(this->header.operation);

            /* TODO: handle endianness when populating PACKED */
            PackedInstruction<addr_bits> packed;
            packed.header.operation = this->header.operation;
            packed.header.width = this->header.width;
            packed.header.constant_mask = this->header.constant_mask;
            packed.header.output = this->header.output;

            switch (info.format()) {
            case InstructionFormat::NoArgs:
                out.write(reinterpret_cast<const char*>(&packed), sizeof(packed.header));
                break;
            case InstructionFormat::OneArg:
                packed.one_arg.input1 = this->one_arg.input1;
                out.write(reinterpret_cast<const char*>(&packed), sizeof(packed.header) + sizeof(packed.one_arg));
                break;
            case InstructionFormat::TwoArgs:
                packed.two_args.input1 = this->two_args.input1;
                packed.two_args.input2 = this->two_args.input2;
                out.write(reinterpret_cast<const char*>(&packed), sizeof(packed.header) + sizeof(packed.two_args));
                break;
            case InstructionFormat::ThreeArgs:
                packed.three_args.input1 = this->three_args.input1;
                packed.three_args.input2 = this->three_args.input2;
                packed.three_args.input3 = this->three_args.input3;
                out.write(reinterpret_cast<const char*>(&packed), sizeof(packed.header) + sizeof(packed.three_args));
                break;
            case InstructionFormat::Constant:
                packed.constant.constant = this->constant.constant;
                out.write(reinterpret_cast<const char*>(&packed), sizeof(packed.header) + sizeof(packed.constant));
                break;
            default:
                std::abort();
            }
        }

        // void write_to_output(std::ostream& out) const {
        //     OpInfo info(this->header.operation);
        //     util::write_lower_bytes(out, static_cast<std::uint8_t>(this->header.operation), sizeof(this->header.operation));
        //     util::write_lower_bytes(out, this->header.width, sizeof(this->header.width));
        //     util::write_lower_bytes(out, this->header.constant_mask, sizeof(this->header.constant_mask));
        //     util::write_lower_bytes(out, this->header.output, addr_bytes);
        //
        //     switch (info.format()) {
        //     case InstructionFormat::NoArgs:
        //         break;
        //     case InstructionFormat::OneArg:
        //         util::write_lower_bytes(out, this->one_arg.input1, addr_bytes);
        //         break;
        //     case InstructionFormat::TwoArgs:
        //         util::write_lower_bytes(out, this->two_args.input1, addr_bytes);
        //         util::write_lower_bytes(out, this->two_args.input2, addr_bytes);
        //         break;
        //     case InstructionFormat::ThreeArgs:
        //         util::write_lower_bytes(out, this->three_args.input1, addr_bytes);
        //         util::write_lower_bytes(out, this->three_args.input2, addr_bytes);
        //         util::write_lower_bytes(out, this->three_args.input3, addr_bytes);
        //         break;
        //     case InstructionFormat::Constant:
        //         util::write_lower_bytes(out, this->constant.constant, sizeof(this->constant.constant));
        //         break;
        //     default:
        //         std::abort();
        //     }
        // }

        bool read_from_input(std::istream& in) {
            util::read_lower_bytes(in, *reinterpret_cast<std::uint8_t*>(&this->header.operation), sizeof(this->header.operation));
            util::read_lower_bytes(in, this->header.width, sizeof(this->header.width));
            util::read_lower_bytes(in, this->header.constant_mask, sizeof(this->header.constant_mask));
            util::read_lower_bytes(in, this->header.output, addr_bytes);

            OpInfo info(this->header.operation);
            switch (info.format()) {
            case InstructionFormat::NoArgs:
                break;
            case InstructionFormat::OneArg:
                util::read_lower_bytes(in, this->one_arg.input1, addr_bytes);
                break;
            case InstructionFormat::TwoArgs:
                util::read_lower_bytes(in, this->two_args.input1, addr_bytes);
                util::read_lower_bytes(in, this->two_args.input2, addr_bytes);
                break;
            case InstructionFormat::ThreeArgs:
                util::read_lower_bytes(in, this->three_args.input1, addr_bytes);
                util::read_lower_bytes(in, this->three_args.input2, addr_bytes);
                util::read_lower_bytes(in, this->three_args.input3, addr_bytes);
                break;
            case InstructionFormat::Constant:
                util::read_lower_bytes(in, this->constant.constant, sizeof(this->constant.constant));
                break;
            default:
                std::abort();
            }

            return in.good();
        }
    };

    using VirtInstruction = Instruction<56>;
    using PhysInstruction = Instruction<40>;
}

#endif
