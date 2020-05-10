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

#ifndef MAGE_MEMPROG_INSTRUCTION_HPP_
#define MAGE_MEMPROG_INSTRUCTION_HPP_

#include <cstddef>
#include <cstdint>
#include "memprog/addr.hpp"
#include "memprog/opcode.hpp"
#include "util/binary.hpp"

namespace mage::memprog {
    using BitWidth = std::uint8_t;

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

    template <std::uint8_t addr_bits, std::uint8_t storage_bits>
    struct PackedInstruction {
        static const constexpr std::uint8_t addr_bytes = addr_bits >> 3;
        static_assert((addr_bits & 0x7) == 0, "Address width not a whole number of bytes");

        struct {
            OpCode operation;
            std::uint8_t flags;
            std::uint64_t output : addr_bits;
        } __attribute__((packed)) header;
        union {
            struct {
                BitWidth width;
            } __attribute__((packed)) no_args;
            struct {
                BitWidth width;
                std::uint64_t input1 : addr_bits;
            } __attribute__((packed)) one_arg;
            struct {
                BitWidth width;
                std::uint64_t input1 : addr_bits;
                std::uint64_t input2 : addr_bits;
            } __attribute__((packed)) two_args;
            struct {
                BitWidth width;
                std::uint64_t input1 : addr_bits;
                std::uint64_t input2 : addr_bits;
                std::uint64_t input3 : addr_bits;
            } __attribute__((packed)) three_args;
            struct {
                BitWidth width;
                std::uint64_t constant;
            } __attribute__((packed)) constant;
            struct {
                std::uint64_t storage : storage_bits;
            } __attribute__((packed)) swap;
        };

        static constexpr std::size_t size(InstructionFormat format) {
            switch (format) {
            case InstructionFormat::NoArgs:
                return sizeof(PackedInstruction<addr_bits, storage_bits>::header) + sizeof(PackedInstruction<addr_bits, storage_bits>::no_args);
            case InstructionFormat::OneArg:
                return sizeof(PackedInstruction<addr_bits, storage_bits>::header) + sizeof(PackedInstruction<addr_bits, storage_bits>::one_arg);
            case InstructionFormat::TwoArgs:
                return sizeof(PackedInstruction<addr_bits, storage_bits>::header) + sizeof(PackedInstruction<addr_bits, storage_bits>::two_args);
            case InstructionFormat::ThreeArgs:
                return sizeof(PackedInstruction<addr_bits, storage_bits>::header) + sizeof(PackedInstruction<addr_bits, storage_bits>::three_args);
            case InstructionFormat::Constant:
                return sizeof(PackedInstruction<addr_bits, storage_bits>::header) + sizeof(PackedInstruction<addr_bits, storage_bits>::constant);
            case InstructionFormat::Swap:
                return sizeof(PackedInstruction<addr_bits, storage_bits>::header) + sizeof(PackedInstruction<addr_bits, storage_bits>::swap);
            default:
                std::abort();
            }
        }
        static constexpr std::size_t size(OpCode operation) {
            OpInfo info(operation);
            return PackedInstruction<addr_bits, storage_bits>::size(info.format());
        }
        constexpr std::size_t size() const {
            return PackedInstruction<addr_bits, storage_bits>::size(this->header.operation);
        }

        /* Useful if we memory-map a program. */
        constexpr PackedInstruction<addr_bits, storage_bits>* next(std::size_t size) {
            std::uint8_t* self = reinterpret_cast<std::uint8_t*>(this);
            return reinterpret_cast<PackedInstruction<addr_bits, storage_bits>*>(self + size);
        }
        constexpr PackedInstruction<addr_bits, storage_bits>* next(InstructionFormat format) {
            return this->next(PackedInstruction<addr_bits, storage_bits>::size(format));
        }
        constexpr PackedInstruction<addr_bits, storage_bits>* next() {
            OpInfo info(this->header.operation);
            return this->next(info.format());
        }
        constexpr const PackedInstruction<addr_bits, storage_bits>* next(std::size_t size) const {
            const std::uint8_t* self = reinterpret_cast<const std::uint8_t*>(this);
            return reinterpret_cast<const PackedInstruction<addr_bits, storage_bits>*>(self + size);
        }
        constexpr const PackedInstruction<addr_bits, storage_bits>* next(InstructionFormat format) const {
            return this->next(PackedInstruction<addr_bits, storage_bits>::size(format));
        }
        constexpr const PackedInstruction<addr_bits, storage_bits>* next() const {
            OpInfo info(this->header.operation);
            return this->next(info.format());
        }

        std::uint8_t store_page_numbers(std::uint64_t* into, PageShift page_shift) {
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

        template <std::uint8_t other_addr_bits, std::uint8_t other_storage_bits>
        std::uint8_t restore_page_numbers(const PackedInstruction<other_addr_bits, other_storage_bits>& original, const std::uint64_t* from, PageShift page_shift) {
            OpInfo info(this->header.operation);
            int num_args = info.num_args();

            std::uint8_t num_pages = 0;

            std::uint64_t output_vpn = pg_num(original.header.output, page_shift);
            this->header.output = pg_set_num(original.header.output, from[num_pages++], page_shift);
            if (num_args > 0) {
                std::uint64_t input1_vpn = pg_num(original.three_args.input1, page_shift);
                if (input1_vpn == output_vpn) {
                    this->three_args.input1 = pg_copy_num(original.three_args.input1, this->header.output, page_shift);
                } else {
                    this->three_args.input1 = pg_set_num(original.three_args.input1, from[num_pages++], page_shift);
                }
                if (num_args > 1) {
                    std::uint64_t input2_vpn = pg_num(original.three_args.input2, page_shift);
                    if (input2_vpn == output_vpn) {
                        this->three_args.input2 = pg_copy_num(original.three_args.input2, this->header.output, page_shift);
                    } else if (input2_vpn == input1_vpn) {
                        this->three_args.input2 = pg_copy_num(original.three_args.input2, this->three_args.input1, page_shift);
                    } else {
                        this->three_args.input2 = pg_set_num(original.three_args.input2, from[num_pages++], page_shift);
                    }
                    if (num_args > 2) {
                        std::uint64_t input3_vpn = pg_num(original.three_args.input3, page_shift);
                        if (input3_vpn == output_vpn) {
                            this->three_args.input3 = pg_copy_num(original.three_args.input3, this->header.output, page_shift);
                        } else if (input3_vpn == input1_vpn) {
                            this->three_args.input3 = pg_copy_num(original.three_args.input3, this->three_args.input1, page_shift);
                        } else if (input3_vpn == input2_vpn) {
                            this->three_args.input3 = pg_copy_num(original.three_args.input3, this->three_args.input2, page_shift);
                        } else {
                            this->three_args.input3 = pg_set_num(original.three_args.input3, from[num_pages++], page_shift);
                        }
                    }
                }
            }
            return num_pages;
        }
    } __attribute__((packed));

    struct Instruction {
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
            struct {
                std::uint64_t constant;
            } constant;
            struct {
                std::uint64_t storage;
            } swap;
        };

        template <std::uint8_t addr_bits, std::uint8_t storage_bits>
        std::size_t pack(PackedInstruction<addr_bits, storage_bits>& packed, InstructionFormat format) const {
            packed.header.operation = this->header.operation;
            packed.header.flags = this->header.flags;
            packed.header.output = this->header.output;

            switch (format) {
            case InstructionFormat::NoArgs:
                packed.no_args.width = this->header.width;
                return sizeof(packed.header) + sizeof(packed.no_args);
            case InstructionFormat::OneArg:
                packed.one_arg.width = this->header.width;
                packed.one_arg.input1 = this->one_arg.input1;
                return sizeof(packed.header) + sizeof(packed.one_arg);
            case InstructionFormat::TwoArgs:
                packed.two_args.width = this->header.width;
                packed.two_args.input1 = this->two_args.input1;
                packed.two_args.input2 = this->two_args.input2;
                return sizeof(packed.header) + sizeof(packed.two_args);
            case InstructionFormat::ThreeArgs:
                packed.three_args.width = this->header.width;
                packed.three_args.input1 = this->three_args.input1;
                packed.three_args.input2 = this->three_args.input2;
                packed.three_args.input3 = this->three_args.input3;
                return sizeof(packed.header) + sizeof(packed.three_args);
            case InstructionFormat::Constant:
                packed.constant.width = this->header.width;
                packed.constant.constant = this->constant.constant;
                return sizeof(packed.header) + sizeof(packed.constant);
            case InstructionFormat::Swap:
                packed.swap.storage = this->swap.storage;
            default:
                std::abort();
            }
        }

        template <std::uint8_t addr_bits, std::uint8_t storage_bits>
        std::size_t pack(PackedInstruction<addr_bits, storage_bits>& packed) const {
            OpInfo info(this->header.operation);
            return this->pack<addr_bits, storage_bits>(packed, info.format());
        }

        template <std::uint8_t addr_bits, std::uint8_t storage_bits>
        std::size_t write_to_output(std::ostream& out) const {
            PackedInstruction<addr_bits, storage_bits> packed;
            std::size_t size = this->pack(packed);
            out.write(reinterpret_cast<const char*>(&packed), size);
            return size;
        }

        template <std::uint8_t addr_bits, std::uint8_t storage_bits>
        std::size_t read_from_input(std::istream& in) {
            /* TODO: handle endianness when populating PACKED */
            PackedInstruction<addr_bits, storage_bits> packed;
            in.read(reinterpret_cast<char*>(&packed.header), sizeof(packed.header));

            this->header.operation = packed.header.operation;
            this->header.flags = packed.header.flags;
            this->header.output = packed.header.output;

            OpInfo info(this->header.operation);
            switch (info.format()) {
            case InstructionFormat::NoArgs:
                this->header.width = packed.no_args.width;
                return sizeof(packed.header);
            case InstructionFormat::OneArg:
                in.read(reinterpret_cast<char*>(&packed.one_arg), sizeof(packed.one_arg));
                this->header.width = packed.one_arg.width;
                this->one_arg.input1 = packed.one_arg.input1;
                return sizeof(packed.header) + sizeof(packed.one_arg);
            case InstructionFormat::TwoArgs:
                in.read(reinterpret_cast<char*>(&packed.two_args), sizeof(packed.two_args));
                this->header.width = packed.two_args.width;
                this->two_args.input1 = packed.two_args.input1;
                this->two_args.input2 = packed.two_args.input2;
                return sizeof(packed.header) + sizeof(packed.two_args);
            case InstructionFormat::ThreeArgs:
                in.read(reinterpret_cast<char*>(&packed.three_args), sizeof(packed.three_args));
                this->header.width = packed.three_args.width;
                this->three_args.input1 = packed.three_args.input1;
                this->three_args.input2 = packed.three_args.input2;
                this->three_args.input3 = packed.three_args.input3;
                return sizeof(packed.header) + sizeof(packed.three_args);
            case InstructionFormat::Constant:
                in.read(reinterpret_cast<char*>(&packed.constant), sizeof(packed.constant));
                this->header.width = packed.constant.width;
                this->constant.constant = packed.constant.constant;
                return sizeof(packed.header) + sizeof(packed.constant);
            case InstructionFormat::Swap:
                in.read(reinterpret_cast<char*>(&packed.swap), sizeof(packed.swap));
                this->swap.storage = packed.swap.storage;
                return sizeof(packed.header) + sizeof(packed.swap);
            default:
                std::abort();
            }
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

    using PackedVirtInstruction = PackedInstruction<virtual_address_bits, virtual_address_bits>;
    using PackedPhysInstruction = PackedInstruction<physical_address_bits, storage_address_bits>;
}

#endif
