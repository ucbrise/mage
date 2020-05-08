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
#include "memprog/addr.hpp"
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

    int instruction_format_num_args(InstructionFormat format);
    bool instruction_format_uses_constant(InstructionFormat format);

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

    enum InstructionFlags : std::uint8_t {
        FlagInput1Constant = 0x1,
        FlagInput2Constant = 0x2,
        FlagInput3Constant = 0x3,
        FlagOutputPageFirstUse = 0x4
    };

    template <InstructionFlags... flags>
    constexpr std::uint8_t instr_flags() {
        return (... | static_cast<std::uint8_t>(flags));
    }

    template <std::uint8_t addr_bits>
    struct PackedInstruction {
        static const constexpr std::uint8_t addr_bytes = addr_bits >> 3;
        static_assert((addr_bits & 0x7) == 0, "Address width not a whole number of bytes");

        struct {
            OpCode operation;
            BitWidth width;
            std::uint8_t flags;
            std::uint64_t output : addr_bits;
        } __attribute__((packed)) header;
        union {
            struct {
                InstructionFormat format;
                std::uint8_t next[0];
            } __attribute__((packed)) no_args;
            struct {
                std::uint64_t constant;
                InstructionFormat format;
                std::uint8_t next[0];
            } __attribute__((packed)) constant;
            struct {
                std::uint64_t input1 : addr_bits;
                InstructionFormat format;
                std::uint8_t next[0];
            } __attribute__((packed)) one_arg;
            struct {
                std::uint64_t input1 : addr_bits;
                std::uint64_t input2 : addr_bits;
                InstructionFormat format;
                std::uint8_t next[0];
            } __attribute__((packed)) two_args;
            struct {
                std::uint64_t input1 : addr_bits;
                std::uint64_t input2 : addr_bits;
                std::uint64_t input3 : addr_bits;
                InstructionFormat format;
                std::uint8_t next[0];
            } __attribute__((packed)) three_args;
        };

        /* Useful if we memory-map a program. */
        PackedInstruction<addr_bits>* next(InstructionFormat& format) {
            OpInfo info(this->op);
            std::uint8_t* rv;
            format = info.format();
            switch (format) {
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

        /* Useful if we memory-map a program or read a program backwards. */
        PackedInstruction<addr_bits>* prev(InstructionFormat& format) {
            std::uint8_t* rv;
            std::uint8_t* self = reinterpret_cast<std::uint8_t*>(this);
            format = static_cast<InstructionFormat>(*(self - sizeof(InstructionFormat)));
            switch (format) {
            case InstructionFormat::NoArgs:
                rv = self - sizeof(this->no_args) - sizeof(this->header);
                break;
            case InstructionFormat::OneArg:
                rv = self - sizeof(this->one_arg) - sizeof(this->header);
                break;
            case InstructionFormat::TwoArgs:
                rv = self - sizeof(this->two_args) - sizeof(this->header);
                break;
            case InstructionFormat::ThreeArgs:
                rv = self - sizeof(this->three_args) - sizeof(this->header);
                break;
            case InstructionFormat::Constant:
                rv = self - sizeof(this->constant) - sizeof(this->header);
                break;
            default:
                std::abort();
            }
            return reinterpret_cast<PackedInstruction<addr_bits>*>(rv);
        }

        std::uint8_t get_requisite_page_numbers(std::uint64_t* into, PageShift page_shift) {
            OpInfo info(this->header.operation);
            int num_args = info.num_args();

            std::uint8_t num_pages = 0;

            std::uint64_t output_vpn = pg_num(this->header.output, page_shift);
            into[num_pages++] = output_vpn;
            if (num_args > 0) {
                std::uint64_t input1_vpn = pg_num(this->three_args.input1, page_shift);
                if (input1_vpn != output_vpn) {
                    into[num_pages++] = input1_vpn;
                }
                if (num_args > 1) {
                    std::uint64_t input2_vpn = pg_num(this->three_args.input2, page_shift);
                    if (input2_vpn != output_vpn && input2_vpn != input1_vpn) {
                        into[num_pages++] = input2_vpn;
                    }
                    if (num_args > 2) {
                        std::uint64_t input3_vpn = pg_num(this->three_args.input3, page_shift);
                        if (input3_vpn != output_vpn && input3_vpn != input1_vpn && input3_vpn != input2_vpn) {
                            into[num_pages++] = input3_vpn;
                        }
                    }
                }
            }

            return num_pages;
        }
    } __attribute__((packed));

    template <std::uint8_t addr_bits>
    struct Instruction {
        static const constexpr std::uint8_t addr_bytes = addr_bits >> 3;
        static_assert((addr_bits & 0x7) == 0, "Address width not a whole number of bytes");

        struct {
            OpCode operation;
            BitWidth width;
            std::uint8_t flags;
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
            packed.header.flags = this->header.flags;
            packed.header.output = this->header.output;

            InstructionFormat format = info.format();
            switch (format) {
            case InstructionFormat::NoArgs:
                packed.no_args.format = format;
                out.write(reinterpret_cast<const char*>(&packed), sizeof(packed.header) + sizeof(packed.no_args));
                break;
            case InstructionFormat::OneArg:
                packed.one_arg.input1 = this->one_arg.input1;
                packed.one_arg.format = format;
                out.write(reinterpret_cast<const char*>(&packed), sizeof(packed.header) + sizeof(packed.one_arg));
                break;
            case InstructionFormat::TwoArgs:
                packed.two_args.input1 = this->two_args.input1;
                packed.two_args.input2 = this->two_args.input2;
                packed.two_args.format = format;
                out.write(reinterpret_cast<const char*>(&packed), sizeof(packed.header) + sizeof(packed.two_args));
                break;
            case InstructionFormat::ThreeArgs:
                packed.three_args.input1 = this->three_args.input1;
                packed.three_args.input2 = this->three_args.input2;
                packed.three_args.input3 = this->three_args.input3;
                packed.three_args.format = format;
                out.write(reinterpret_cast<const char*>(&packed), sizeof(packed.header) + sizeof(packed.three_args));
                break;
            case InstructionFormat::Constant:
                packed.constant.constant = this->constant.constant;
                packed.constant.format = format;
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
        //     util::write_lower_bytes(out, this->header.flags, sizeof(this->header.flags));
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
            /* TODO: handle endianness when populating PACKED */
            PackedInstruction<addr_bits> packed;
            in.read(reinterpret_cast<char*>(&packed.header), sizeof(packed.header));

            this->header.operation = packed.header.operation;
            this->header.width = packed.header.width;
            this->header.flags = packed.header.flags;
            this->header.output = packed.header.output;

            OpInfo info(this->header.operation);
            switch (info.format()) {
            case InstructionFormat::NoArgs:
                break;
            case InstructionFormat::OneArg:
                in.read(reinterpret_cast<char*>(&packed.one_arg), sizeof(packed.one_arg));
                this->one_arg.input1 = packed.one_arg.input1;
                break;
            case InstructionFormat::TwoArgs:
                in.read(reinterpret_cast<char*>(&packed.two_args), sizeof(packed.two_args));
                this->two_args.input1 = packed.two_args.input1;
                this->two_args.input2 = packed.two_args.input2;
                break;
            case InstructionFormat::ThreeArgs:
                in.read(reinterpret_cast<char*>(&packed.three_args), sizeof(packed.three_args));
                this->three_args.input1 = packed.three_args.input1;
                this->three_args.input2 = packed.three_args.input2;
                this->three_args.input3 = packed.three_args.input3;
                break;
            case InstructionFormat::Constant:
                in.read(reinterpret_cast<char*>(&packed.constant), sizeof(packed.constant));
                this->constant.constant = packed.constant.constant;
                break;
            default:
                std::abort();
            }

            return in.good();
        }

        // bool read_from_input(std::istream& in) {
        //     util::read_lower_bytes(in, *reinterpret_cast<std::uint8_t*>(&this->header.operation), sizeof(this->header.operation));
        //     util::read_lower_bytes(in, this->header.width, sizeof(this->header.width));
        //     util::read_lower_bytes(in, this->header.flags, sizeof(this->header.flags));
        //     util::read_lower_bytes(in, this->header.output, addr_bytes);
        //
        //     OpInfo info(this->header.operation);
        //     switch (info.format()) {
        //     case InstructionFormat::NoArgs:
        //         break;
        //     case InstructionFormat::OneArg:
        //         util::read_lower_bytes(in, this->one_arg.input1, addr_bytes);
        //         break;
        //     case InstructionFormat::TwoArgs:
        //         util::read_lower_bytes(in, this->two_args.input1, addr_bytes);
        //         util::read_lower_bytes(in, this->two_args.input2, addr_bytes);
        //         break;
        //     case InstructionFormat::ThreeArgs:
        //         util::read_lower_bytes(in, this->three_args.input1, addr_bytes);
        //         util::read_lower_bytes(in, this->three_args.input2, addr_bytes);
        //         util::read_lower_bytes(in, this->three_args.input3, addr_bytes);
        //         break;
        //     case InstructionFormat::Constant:
        //         util::read_lower_bytes(in, this->constant.constant, sizeof(this->constant.constant));
        //         break;
        //     default:
        //         std::abort();
        //     }
        //
        //     return in.good();
        // }
    };

    using PackedVirtInstruction = PackedInstruction<virtual_address_bits>;
    using VirtInstruction = Instruction<virtual_address_bits>;
    using PackedPhysInstruction = PackedInstruction<physical_address_bits>;
    using PhysInstruction = Instruction<physical_address_bits>;
}

#endif
