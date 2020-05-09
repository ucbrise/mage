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

#ifndef MAGE_MEMPROG_ADDR_H_
#define MAGE_MEMPROG_ADDR_H_

namespace mage::memprog {
    /* VIRTUAL ADDRESSES */

    using VirtAddr = std::uint64_t;
    const constexpr int virtual_address_bits = 56;
    const constexpr VirtAddr invalid_vaddr = (UINT64_C(1) << virtual_address_bits) - 1;
    using VirtPageNumber = std::uint64_t;

    using PageShift = std::uint8_t;
    using PageSize = std::uint64_t;

    inline PageSize pg_size(PageShift shift) {
        return UINT64_C(1) << shift;
    }
    inline PageSize pg_mask(PageShift shift) {
        return pg_size(shift) - 1;
    }

    inline std::uint64_t pg_num(std::uint64_t addr, PageShift shift) {
        return addr >> shift;
    }
    inline std::uint64_t pg_offset(std::uint64_t addr, PageShift shift) {
        return addr & pg_mask(shift);
    }
    inline std::uint64_t pg_next(std::uint64_t addr, PageShift shift) {
        return (pg_num(addr, shift) + 1) << shift;
    }
    inline std::uint64_t pg_base(std::uint64_t addr, PageShift shift) {
        return pg_num(addr, shift) << shift;
    }
    inline std::uint64_t pg_round_up(std::uint64_t addr, PageShift shift) {
        return pg_next(addr - 1, shift);
    }
    inline std::uint64_t pg_round_down(std::uint64_t addr, PageShift shift) {
        return pg_base(addr, shift);
    }
    inline std::uint64_t pg_set_num(std::uint64_t addr, std::uint64_t num, PageShift shift) {
        return (num << shift) | pg_offset(addr, shift);
    }
    inline std::uint64_t pg_copy_num(std::uint64_t addr, std::uint64_t from, PageShift shift) {
        return pg_base(from, shift) | pg_offset(addr, shift);
    }

    /* PHYSICAL ADDRESSES */

    using PhysAddr = std::uint64_t;

    /* Allows for up to 16 TiB of RAM, assuming 16 bytes per wire. */
    const constexpr int physical_address_bits = 40;
    const constexpr PhysAddr invalid_paddr = (UINT64_C(1) << physical_address_bits) - 1;
    using PhysPageNumber = std::uint64_t;

    /* INSTRUCTION NUMBERS */
    using InstructionNumber = std::uint64_t;
    const constexpr int instruction_number_bits = 48;
    const constexpr std::uint64_t invalid_instr = (UINT64_C(1) << instruction_number_bits) - 1;
}

#endif
