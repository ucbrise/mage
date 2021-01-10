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

/**
 * @file instruction.hpp
 * @brief Describes instruction encodings for MAGE's bytecodes.
 */

#ifndef MAGE_INSTRUCTION_HPP_
#define MAGE_INSTRUCTION_HPP_

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include "addr.hpp"
#include "opcode.hpp"

namespace mage {
    /**
     * @brief Integer type large enough to describe the logical width/size of
     * an operation.
     */
    using BitWidth = std::uint16_t;

    /**
     * @brief Flags stored in the instruction encoding. The lower nybble is
     * reserved for use by MAGE's planner, and the upper nybble is preserved
     * planning and can be used to modify the instruction's behavior.
     */
    enum InstructionFlags : std::uint8_t {
        FlagInput1Constant = 0x1,
        FlagInput2Constant = 0x2,
        FlagInput3Constant = 0x4,
        FlagOutputPageFirstUse = 0x8,
        FlagEvaluatorInput = 0x10,
        FlagNotNormalized = 0x20,
    };

    /**
     * @brief Provides a constexpr (compile-time) assembly of instruction flags
     * into a bitmask.
     *
     * @tparam flags The specified instruction flags to combine.
     * @return A constexpr bitmask of the specified instruction flags.
     */
    template <InstructionFlags... flags>
    constexpr std::uint8_t instr_flags() {
        return (... | static_cast<std::uint8_t>(flags));
    }

    /**
     * @brief Structure describing the instruction encoding used in MAGE's
     * bytecodes.
     *
     * @tparam addr_bits Number of bits used to describe a MAGE address. Chosen
     * depending on the size of the memory address space on which the
     * instruction operates.
     * @tparam storage_bits Number of bits used to describe a storage (swap)
     * address. Chosen depending on the size of the storage address space.
     */
    template <std::uint8_t addr_bits, std::uint8_t storage_bits>
    struct PackedInstruction {
        static const constexpr std::uint8_t addr_bytes = addr_bits >> 3;
        static_assert((addr_bits & 0x7) == 0, "Address width not a whole number of bytes");

        /**
         * @brief All instructions have the same header, described by this
         * structure.
         *
         * This allows any instruction to be parsed by reading its header,
         * and determining the structure of the rest of the instruction based
         * on the opcode. The size of the instruction isn't known until the
         * opcode (part of the header) is read.
         */
        struct {
            OpCode operation;
            std::uint8_t flags;
        } __attribute__((packed)) header;

        /**
         * @brief Remainder of the instruction encoding. Each alternative of
         * the union corresponds to an InstructionFormat value.
         *
         * @sa mage::InstructionFormat
         */
        union {
            struct {
                BitWidth width;
                std::uint64_t output : addr_bits;
            } __attribute__((packed)) no_args;
            struct {
                BitWidth width;
                std::uint64_t output : addr_bits;
                std::uint64_t input1 : addr_bits;
            } __attribute__((packed)) one_arg;
            struct {
                BitWidth width;
                std::uint64_t output : addr_bits;
                std::uint64_t input1 : addr_bits;
                std::uint64_t input2 : addr_bits;
            } __attribute__((packed)) two_args;
            struct {
                BitWidth width;
                std::uint64_t output : addr_bits;
                std::uint64_t input1 : addr_bits;
                std::uint64_t input2 : addr_bits;
                std::uint64_t input3 : addr_bits;
            } __attribute__((packed)) three_args;
            struct {
                BitWidth width;
                std::uint64_t output : addr_bits;
                std::uint64_t constant;
            } __attribute__((packed)) constant;
            struct {
                std::uint64_t memory : addr_bits;
                std::uint64_t storage : storage_bits;
            } __attribute__((packed)) swap;
            struct {
                std::uint64_t memory : addr_bits;
            } __attribute__((packed)) swap_finish;
            struct {
                std::uint32_t data;
            } __attribute__((packed)) control;
        };

        /**
         * @brief For a specified instruction format, determines the size of
         * the instruction encoding, in bytes.
         *
         * @param format The specified instruction format.
         * @return The size of the encoded instruction, in bytes.
         */
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
            case InstructionFormat::SwapFinish:
                return sizeof(PackedInstruction<addr_bits, storage_bits>::header) + sizeof(PackedInstruction<addr_bits, storage_bits>::swap_finish);
            case InstructionFormat::Control:
                return sizeof(PackedInstruction<addr_bits, storage_bits>::header) + sizeof(PackedInstruction<addr_bits, storage_bits>::control);
            default:
                std::abort();
            }
        }

        /**
         * @brief Obtains the size of the instruction, in bytes, encoding for
         * the specified operation.
         *
         * @param operation The specified operation.
         * @return The size of the encoded instruction, in bytes.
         */
        static constexpr std::size_t size(OpCode operation) {
            OpInfo info(operation);
            return PackedInstruction<addr_bits, storage_bits>::size(info.format());
        }

        /**
         * @brief The size of this instruction's encoding.
         *
         * The size is computed by reading the operation from the encoded
         * header.
         *
         * @return The size of the encoded instruction, in bytes.
         */
        constexpr std::size_t size() const {
            return PackedInstruction<addr_bits, storage_bits>::size(this->header.operation);
        }

        /* Useful if we memory-map a program. */
        // constexpr PackedInstruction<addr_bits, storage_bits>* next(std::size_t size) {
        //     std::uint8_t* self = reinterpret_cast<std::uint8_t*>(this);
        //     return reinterpret_cast<PackedInstruction<addr_bits, storage_bits>*>(self + size);
        // }
        // constexpr PackedInstruction<addr_bits, storage_bits>* next(InstructionFormat format) {
        //     return this->next(PackedInstruction<addr_bits, storage_bits>::size(format));
        // }
        // constexpr PackedInstruction<addr_bits, storage_bits>* next() {
        //     OpInfo info(this->header.operation);
        //     return this->next(info.format());
        // }
        // constexpr const PackedInstruction<addr_bits, storage_bits>* next(std::size_t size) const {
        //     const std::uint8_t* self = reinterpret_cast<const std::uint8_t*>(this);
        //     return reinterpret_cast<const PackedInstruction<addr_bits, storage_bits>*>(self + size);
        // }
        // constexpr const PackedInstruction<addr_bits, storage_bits>* next(InstructionFormat format) const {
        //     return this->next(PackedInstruction<addr_bits, storage_bits>::size(format));
        // }
        // constexpr const PackedInstruction<addr_bits, storage_bits>* next() const {
        //     OpInfo info(this->header.operation);
        //     return this->next(info.format());
        // }

        /**
         * @brief Stores the page numbers of this instruction's arguments'
         * addresses requiring address translation into the specified array,
         * starting with this instruction's output.
         *
         * @sa restore_page_numbers()
         *
         * @param into A pointer to the array into which to store the page
         * numbers.
         * @param page_shift Describes the size of pages used.
         * @return The number of pages stored in the array.
         */
        std::uint8_t store_page_numbers(std::uint64_t* into, PageShift page_shift) {
            OpInfo info(this->header.operation);

            switch (info.format()) {
            case InstructionFormat::NoArgs:
            case InstructionFormat::OneArg:
            case InstructionFormat::TwoArgs:
            case InstructionFormat::ThreeArgs:
            case InstructionFormat::Constant:
            {
                int num_args = info.num_args();
                std::uint8_t num_pages = 0;

                std::uint64_t output_vpn = pg_num(this->three_args.output, page_shift);
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
            case InstructionFormat::Swap:
            case InstructionFormat::SwapFinish:
            case InstructionFormat::Control:
                return 0;
            default:
                std::abort();
            }
        }

        /**
         * @brief Sets this instruction's arguments' addresses by reading the
         * page numbers from the specified array and reading the page offsets
         * from the specified instruction encoding.
         *
         * An intended use case is to read the page numbers in an instruction
         * usin store_page_numbers(), translate the address space using those
         * page numbers to obtain new page numbers, and then use this function
         * to encode an instruction that uses translated addresses.
         *
         * @sa store_page_numbers()
         *
         * @param original The instruction encoding from which to read page
         * offsets.
         * @param from A pointer to the array from which to read page numbers.
         * @param page_shift Describes the size of pages used.
         * @return The number of pages read from the array.
         */
        template <std::uint8_t other_addr_bits, std::uint8_t other_storage_bits>
        std::uint8_t restore_page_numbers(const PackedInstruction<other_addr_bits, other_storage_bits>& original, const std::uint64_t* from, PageShift page_shift) {
            OpInfo info(this->header.operation);

            switch (info.format()) {
            case InstructionFormat::NoArgs:
            case InstructionFormat::OneArg:
            case InstructionFormat::TwoArgs:
            case InstructionFormat::ThreeArgs:
            case InstructionFormat::Constant:
            {
                int num_args = info.num_args();
                std::uint8_t num_pages = 0;

                std::uint64_t output_vpn = pg_num(original.three_args.output, page_shift);
                std::uint64_t output_ppn = from[num_pages++];
                this->three_args.output = pg_set_num(original.three_args.output, output_ppn, page_shift);
                if (num_args > 0) {
                    std::uint64_t input1_vpn = pg_num(original.three_args.input1, page_shift);
                    std::uint64_t input1_ppn = input1_vpn == output_vpn ? output_ppn : from[num_pages++];
                    this->three_args.input1 = pg_set_num(original.three_args.input1, input1_ppn, page_shift);
                    if (num_args > 1) {
                        std::uint64_t input2_vpn = pg_num(original.three_args.input2, page_shift);
                        std::uint64_t input2_ppn = input2_vpn == output_vpn ? output_ppn
                            : input2_vpn == input1_vpn ? input1_ppn
                            : from[num_pages++];
                        this->three_args.input2 = pg_set_num(original.three_args.input2, input2_ppn, page_shift);
                        if (num_args > 2) {
                            std::uint64_t input3_vpn = pg_num(original.three_args.input3, page_shift);
                            std::uint64_t input3_ppn = input3_vpn == output_vpn ? output_ppn
                                : input3_vpn == input1_vpn ? input1_ppn
                                : input3_vpn == input2_vpn ? input2_ppn
                                : from[num_pages++];
                            this->three_args.input3 = pg_set_num(original.three_args.input3, input3_ppn, page_shift);
                        }
                    }
                } else if (info.uses_constant()) {
                    this->constant.constant = original.constant.constant;
                }
                return num_pages;
            }
            case InstructionFormat::Swap:
            case InstructionFormat::SwapFinish:
                std::abort(); // shouldn't be necessary for these instruction types
            case InstructionFormat::Control:
                this->control.data = original.control.data;
                return 0;
            default:
                std::abort();
            }
        }
    } __attribute__((packed));

    /**
     * @brief Logical instruction format that is easier to use than
     * PackedInstruction, but does not reflect the actual instruction encoding.
     */
    struct Instruction {
        struct {
            OpCode operation;
            BitWidth width;
            std::uint8_t flags;
            std::uint64_t output;
        } header;

        union {
            struct {
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
            struct {
            } swap_finish;
            struct {
                std::uint32_t data;
            } control;
        };

        /**
         * @brief Writes the data in this Instruction structure into a
         * PackedInstruction, following the provided instruction format.
         *
         * @tparam addr_bits,storage_bits Template arguments of the specified
         * PackedInstruction.
         * @param packed The PackedInstruction into which to write the data in
         * this Instruction structure.
         * @param format The instruction format to follow when writing data
         * into the PackedInstruction structure.
         */
        template <std::uint8_t addr_bits, std::uint8_t storage_bits>
        std::size_t pack(PackedInstruction<addr_bits, storage_bits>& packed, InstructionFormat format) const {
            packed.header.operation = this->header.operation;
            packed.header.flags = this->header.flags;

            switch (format) {
            case InstructionFormat::NoArgs:
                packed.no_args.width = this->header.width;
                packed.no_args.output = this->header.output;
                return sizeof(packed.header) + sizeof(packed.no_args);
            case InstructionFormat::OneArg:
                packed.one_arg.width = this->header.width;
                packed.one_arg.output = this->header.output;
                packed.one_arg.input1 = this->one_arg.input1;
                return sizeof(packed.header) + sizeof(packed.one_arg);
            case InstructionFormat::TwoArgs:
                packed.two_args.width = this->header.width;
                packed.two_args.output = this->header.output;
                packed.two_args.input1 = this->two_args.input1;
                packed.two_args.input2 = this->two_args.input2;
                return sizeof(packed.header) + sizeof(packed.two_args);
            case InstructionFormat::ThreeArgs:
                packed.three_args.width = this->header.width;
                packed.three_args.output = this->header.output;
                packed.three_args.input1 = this->three_args.input1;
                packed.three_args.input2 = this->three_args.input2;
                packed.three_args.input3 = this->three_args.input3;
                return sizeof(packed.header) + sizeof(packed.three_args);
            case InstructionFormat::Constant:
                packed.constant.width = this->header.width;
                packed.constant.output = this->header.output;
                packed.constant.constant = this->constant.constant;
                return sizeof(packed.header) + sizeof(packed.constant);
            case InstructionFormat::Swap:
                packed.swap.memory = this->header.output;
                packed.swap.storage = this->swap.storage;
                return sizeof(packed.header) + sizeof(packed.swap);
            case InstructionFormat::SwapFinish:
                packed.swap_finish.memory = this->header.output;
                return sizeof(packed.header) + sizeof(packed.swap_finish);
            case InstructionFormat::Control:
                packed.control.data = this->control.data;
                return sizeof(packed.header) + sizeof(packed.control);
            default:
                std::abort();
            }
        }

        /**
         * @brief Writes the data in this Instruction structure into a
         * PackedInstruction, following the instruction format implied by this
         * Instruction structure's operation.
         *
         * @tparam addr_bits,storage_bits Template arguments of the specified
         * PackedInstruction.
         * @param packed The PackedInstruction into which to write the data in
         * this Instruction structure.
         */
        template <std::uint8_t addr_bits, std::uint8_t storage_bits>
        std::size_t pack(PackedInstruction<addr_bits, storage_bits>& packed) const {
            OpInfo info(this->header.operation);
            return this->pack<addr_bits, storage_bits>(packed, info.format());
        }

        // template <std::uint8_t addr_bits, std::uint8_t storage_bits>
        // std::size_t write_to_output(std::ostream& out) const {
        //     PackedInstruction<addr_bits, storage_bits> packed;
        //     std::size_t size = this->pack(packed);
        //     out.write(reinterpret_cast<const char*>(&packed), size);
        //     return size;
        // }

        // template <std::uint8_t addr_bits, std::uint8_t storage_bits>
        // std::size_t read_from_input(std::istream& in) {
        //     /* TODO: handle endianness when populating PACKED */
        //     PackedInstruction<addr_bits, storage_bits> packed;
        //     in.read(reinterpret_cast<char*>(&packed.header), sizeof(packed.header));
        //
        //     this->header.operation = packed.header.operation;
        //     this->header.flags = packed.header.flags;
        //     this->header.output = packed.header.output;
        //
        //     OpInfo info(this->header.operation);
        //     switch (info.format()) {
        //     case InstructionFormat::NoArgs:
        //         this->header.width = packed.no_args.width;
        //         return sizeof(packed.header);
        //     case InstructionFormat::OneArg:
        //         in.read(reinterpret_cast<char*>(&packed.one_arg), sizeof(packed.one_arg));
        //         this->header.width = packed.one_arg.width;
        //         this->one_arg.input1 = packed.one_arg.input1;
        //         return sizeof(packed.header) + sizeof(packed.one_arg);
        //     case InstructionFormat::TwoArgs:
        //         in.read(reinterpret_cast<char*>(&packed.two_args), sizeof(packed.two_args));
        //         this->header.width = packed.two_args.width;
        //         this->two_args.input1 = packed.two_args.input1;
        //         this->two_args.input2 = packed.two_args.input2;
        //         return sizeof(packed.header) + sizeof(packed.two_args);
        //     case InstructionFormat::ThreeArgs:
        //         in.read(reinterpret_cast<char*>(&packed.three_args), sizeof(packed.three_args));
        //         this->header.width = packed.three_args.width;
        //         this->three_args.input1 = packed.three_args.input1;
        //         this->three_args.input2 = packed.three_args.input2;
        //         this->three_args.input3 = packed.three_args.input3;
        //         return sizeof(packed.header) + sizeof(packed.three_args);
        //     case InstructionFormat::Constant:
        //         in.read(reinterpret_cast<char*>(&packed.constant), sizeof(packed.constant));
        //         this->header.width = packed.constant.width;
        //         this->constant.constant = packed.constant.constant;
        //         return sizeof(packed.header) + sizeof(packed.constant);
        //     case InstructionFormat::Swap:
        //         in.read(reinterpret_cast<char*>(&packed.swap), sizeof(packed.swap));
        //         this->swap.storage = packed.swap.storage;
        //         return sizeof(packed.header) + sizeof(packed.swap);
        //     default:
        //         std::abort();
        //     }
        // }

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

    /**
     * @brief Writes the specified instruction to the specified output stream
     * in human-readable form.
     *
     * @param out The output stream to which to write a human-readable
     * representation of the specified instruction instruction.
     * @param p The specified instruction.
     * @return A reference to the specified output stream.
     */
    template<std::uint8_t addr_bits, std::uint8_t storage_bits>
    std::ostream& operator <<(std::ostream& out, const PackedInstruction<addr_bits, storage_bits>& p) {
        const char* name = opcode_to_string(p.header.operation);
        out << name;

        OpInfo info(p.header.operation);
        switch (info.format()) {
        case InstructionFormat::NoArgs:
            out << "<" << (int) p.no_args.width << ">(" << p.no_args.output << ")";
            break;
        case InstructionFormat::OneArg:
            out << "<" << (int) p.one_arg.width << ">(" << p.one_arg.output << ", " << p.one_arg.input1 << ")";
            break;
        case InstructionFormat::TwoArgs:
            out << "<" << (int) p.two_args.width << ">(" << p.two_args.output << ", " << p.two_args.input1 << ", " << p.two_args.input2 << ")";
            break;
        case InstructionFormat::ThreeArgs:
            out << "<" << (int) p.three_args.width << ">(" << p.three_args.output << ", " << p.three_args.input1 << ", " << p.three_args.input2 << ", " << p.three_args.input3 << ")";
            break;
        case InstructionFormat::Constant:
            out << "(" << p.constant.output << ", " << p.constant.constant << ")";
            break;
        case InstructionFormat::Swap:
            out << "(" << p.swap.memory << ", " << p.swap.storage << ")";
            break;
        case InstructionFormat::SwapFinish:
            out << "(" << p.swap_finish.memory << ")";
            break;
        case InstructionFormat::Control:
            out << "(" << p.control.data << ")";
            break;
        default:
            std::abort();
        }

        return out;
    }

    /**
     * @brief Instantiation of the PackedInstruction template with address
     * widths suitable for use with the MAGE-virtual address space.
     */
    using PackedVirtInstruction = PackedInstruction<virtual_address_bits, virtual_address_bits>;

    /**
     * @brief Instantiation of the PackedInstruction template with address
     * widths suitable for use with the MAGE-physical address space.
     */
    using PackedPhysInstruction = PackedInstruction<physical_address_bits, storage_address_bits>;
}

#endif
