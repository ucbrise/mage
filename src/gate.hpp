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

#ifndef MAGE_GATE_HPP_
#define MAGE_GATE_HPP_

#include <cassert>
#include <cstdint>
#include "wire.hpp"
#include "locator.hpp"

namespace mage {
    using GatePageID = std::uint64_t;

    //constexpr const std::uint64_t page_magic = UINT64_C(0xfd908b96364a2e73);
    constexpr const std::uint32_t gate_page_magic = UINT32_C(0x347b0c33);

    /* Truth table: MSB corresponds to (0, 0) and LSB corresponds to (1, 1). */
    enum class GateType : std::uint8_t {
        NOT = 0x0,
        AND = 0x1,
        XOR = 0x6,
        OR = 0x7,
        NOR = 0x8,
        XNOR = 0x9,
        NAND = 0xe
    };

    struct GateStructure {
        WireID input1;
        WireID input2;
        WireID output;
    };

    // template <typename T>
    // concept Gate = requires(T a) {
    //     { a.control } -> GateControl&;
    // };

    /*
     * A GatePage has four parts: header, structure, data, and types. Header
     * contains the index at which the other parts begin. Structure contains
     * the GateStructures describing which wires each gate depends on. Types
     * describes the type of each gate. Data contains the garbled tables that
     * some gates depend on.
     */
    struct GatePage {
        std::uint32_t gate_page_magic;
        std::uint32_t num_gates;
        std::uint32_t types_index;
        std::uint32_t data_index;
        std::uint8_t body[0];

        const GateStructure* structure() const {
            assert(this->gate_page_magic == gate_page_magic);
            return reinterpret_cast<const GateStructure*>(&this->body[0]);
        }

        const GateType* types() const {
            assert(this->gate_page_magic == gate_page_magic);
            return reinterpret_cast<const GateType*>(&this->body[this->types_index]);
        }

        const void* data() const {
            assert(this->gate_page_magic == gate_page_magic);
            return reinterpret_cast<const void*>(&this->body[this->data_index]);
        }
    };
}

#endif
