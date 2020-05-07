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

    using PageShift = std::uint8_t;
    using PageSize = std::uint64_t;

    inline PageSize pgsize(PageShift shift) {
        return UINT64_C(1) << shift;
    }
    inline PageSize pgmask(PageShift shift) {
        return pgsize(shift) - 1;
    }
    using VirtPageNumber = std::uint64_t;
    inline VirtPageNumber pgnum(VirtAddr addr, PageShift shift) {
        return addr >> shift;
    }
    inline VirtPageNumber pg_round_up(VirtAddr addr, PageShift shift) {
        return ((addr >> shift) + 1) << shift;
    }
    inline VirtPageNumber pg_round_down(VirtAddr addr, PageShift shift) {
        return addr & ~pgmask(shift);
    }

    /* PHYSICAL ADDRESSES */

    using PhysAddr = std::uint64_t;

    /* Allows for up to 16 TiB of RAM, assuming 16 bytes per wire. */
    const constexpr int physical_address_bits = 40;
    const constexpr PhysAddr invalid_paddr = (UINT64_C(1) << physical_address_bits) - 1;
}

#endif
