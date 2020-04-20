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

#ifndef MAGE_DSL_ANNOTATOR_HPP_
#define MAGE_DSL_ANNOTATOR_HPP_

#include <cstdint>
#include <string>
#include "dsl/program.hpp"

namespace mage::dsl {
    using PhysAddr = std::uint64_t;

    /* Allows for up to 1 TiB of RAM, assuming 16 bytes per wire. */
    const constexpr int physical_address_bits = 36;
    const constexpr PhysAddr invalid_paddr = (UINT64_C(1) << physical_address_bits) - 1;

    struct PhysicalInstruction {
        union {
            struct {
                PhysAddr primary : physical_address_bits;
                VirtAddr secondary : virtual_address_bits;
            };
            struct {
                PhysAddr input1 : physical_address_bits;
                PhysAddr input2 : physical_address_bits;
                PhysAddr input3 : physical_address_bits;
                PhysAddr output : physical_address_bits;
            };
        };
        OpCode operation;
        BitWidth width;
    } __attribute__((packed));

    using VirtualPageNumber = std::uint64_t;

    struct VirtualPageRange {
        VirtualPageNumber start;
        VirtualPageNumber end; // inclusive;
    };

    const constexpr std::uint32_t annotation_header_magic = UINT32_C(0x54ac3429);

    struct InstructionAnnotationHeader {
        std::uint16_t num_input_pages;
        std::uint16_t num_output_pages;
        std::uint32_t magic;
    } __attribute((packed));

    using VirtualPageNumber = std::uint64_t;

    std::uint64_t reverse_annotate_program(std::string reverse_annotated_program, std::string original_program, std::uint8_t page_shift);
    void unreverse_annotations(std::string annotated_program, std::string reverse_annotated_program);
}

#endif
