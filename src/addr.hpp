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
 * @file addr.hpp
 * @brief Address type definitions and utility functions.
 */

#ifndef MAGE_ADDR_H_
#define MAGE_ADDR_H_

#include <cstdint>

namespace mage {
    /* VIRTUAL ADDRESSES */

    /**
     * @brief Integer type that can hold an an address (pointer) in the
     * MAGE-virtual address space.
     */
    using VirtAddr = std::uint64_t;

    /**
     * @brief Size of a MAGE-virtual address, in bits.
     *
     * This should be set large enough that the any reasonable program's memory
     * would fit in the MAGE-virtual address space without swapping any data
     * to secondary storage.
     */
    const constexpr int virtual_address_bits = 56;

    /** @brief Sentinel value representing an invalid MAGE-virtual address. */
    const constexpr VirtAddr invalid_vaddr = (UINT64_C(1) << virtual_address_bits) - 1;

    /**
     * @brief Integer type that can hold a page number in the MAGE-virtual
     * address space.
     */
    using VirtPageNumber = std::uint64_t;

    /**
     * @brief Integer type that can hold the size of the offset, in bits, in a
     * MAGE-virtual address or MAGE-physical address.
     */
    using PageShift = std::uint8_t;

    /**
     * @brief Integer type that can hold the size of a page in a MAGE-virtual
     * address or MAGE-physical address.
     */
    using PageSize = std::uint64_t;

    /**
     * @brief Computes the size of a page.
     *
     * @param shift The number of bits to represent a page offset in
     * MAGE-virtual and MAGE-physical addresses.
     * @return The size of a page.
     */
    inline PageSize pg_size(PageShift shift) {
        return UINT64_C(1) << shift;
    }

    /**
     * @brief Computes a bit mask that selects the offset bits of a
     * MAGE-virtual or MAGE-physical address.
     *
     * @param shift The number of bits to represent a page offset in
     * MAGE-virtual and MAGE-physical addresses.
     * @return A bit mask that selects the offset bits of a MAGE-virtual or
     * MAGE-physical address.
     */
    inline PageSize pg_mask(PageShift shift) {
        return pg_size(shift) - 1;
    }

    /**
     * @brief Computes the address of a page.
     *
     * @param page_number The page number of the specified page in the desired
     * address space.
     * @param shift The number of bits to represent a page offset in
     * MAGE-virtual and MAGE-physical addresses.
     * @return The address of the specified page in the desired address space.
     */
    inline std::uint64_t pg_addr(std::uint64_t page_number, PageShift shift) {
        return page_number << shift;
    }

    /**
     * @brief Computes the page number of the page containing the specified
     * address.
     *
     * @param addr The specified address in the desired address space.
     * @param shift The number number of bits to represent a page offset in
     * MAGE-virtual and MAGE-physical addresses.
     * @return The number of the page containing the specified address in the
     * desired address space.
     */
    inline std::uint64_t pg_num(std::uint64_t addr, PageShift shift) {
        return addr >> shift;
    }

    /**
     * @brief Computes the offset of the specified address within the page that
     * contains it.
     *
     * @param addr The specified address in the desired address space.
     * @param shift The number number of bits to represent a page offset in
     * MAGE-virtual and MAGE-physical addresses.
     * @return The offset of the specified address within the page that
     * contains it.
     */
    inline std::uint64_t pg_offset(std::uint64_t addr, PageShift shift) {
        return addr & pg_mask(shift);
    }

    /**
     * @brief Computes the address of the next page after the one containing
     * the specified address.
     *
     * @param addr The specified address in the desired address space.
     * @param shift The number number of bits to represent a page offset in
     * MAGE-virtual and MAGE-physical addresses.
     * @return The address of the next page after the one containing the
     * specified address.
     */
    inline std::uint64_t pg_next(std::uint64_t addr, PageShift shift) {
        return (pg_num(addr, shift) + 1) << shift;
    }

    /**
     * @brief Computes the address of the page containing the specified
     * address.
     *
     * @param addr The specified address in the desired address space.
     * @param shift The number number of bits to represent a page offset in
     * MAGE-virtual and MAGE-physical addresses.
     * @return The address of the page containing the specified address in the
     * desired address space.
     */
    inline std::uint64_t pg_base(std::uint64_t addr, PageShift shift) {
        return pg_num(addr, shift) << shift;
    }

    /**
     * @brief Rounds the specified address up to the nearest page boundary.
     *
     * @param addr The specified address in the desired address space.
     * @param shift The number number of bits to represent a page offset in
     * MAGE-virtual and MAGE-physical addresses.
     * @return The result of rounding the specified address up to the nearest
     * page boundary.
     */
    inline std::uint64_t pg_round_up(std::uint64_t addr, PageShift shift) {
        return pg_next(addr - 1, shift);
    }

    /**
     * @brief Rounds the specified address down to the nearest page boundary.
     *
     * @param addr The specified address in the desired address space.
     * @param shift The number number of bits to represent a page offset in
     * MAGE-virtual and MAGE-physical addresses.
     * @return The result of rounding the specified address down to the nearest
     * page boundary.
     */
    inline std::uint64_t pg_round_down(std::uint64_t addr, PageShift shift) {
        return pg_base(addr, shift);
    }

    /**
     * @brief Replaces the page number of the specified address with the
     * provided page number, while preserving its offset.
     *
     * @param addr The specified address in the desired address space.
     * @param num The new page number.
     * @param shift The number number of bits to represent a page offset in
     * MAGE-virtual and MAGE-physical addresses.
     * @return The result of replacing the page number of the specified address
     * with the provided page number.
     */
    inline std::uint64_t pg_set_num(std::uint64_t addr, std::uint64_t num, PageShift shift) {
        return (num << shift) | pg_offset(addr, shift);
    }

    /* PHYSICAL ADDRESSES */

    /**
     * @brief Integer type that can hold an an address (pointer) in the
     * MAGE-physical address space.
     */
    using PhysAddr = std::uint64_t;

    /* Allows for up to 16 TiB of RAM, assuming 16 bytes per wire. */
    /**
     * @brief Size of a MAGE-physical address, in bits.
     *
     * This should be set large enough to contain any reasonable target
     * machine's physical memory.
     */
    const constexpr int physical_address_bits = 40;

    /** @brief Sentinel value representing an invalid MAGE-physical address. */
    const constexpr PhysAddr invalid_paddr = (UINT64_C(1) << physical_address_bits) - 1;

    /**
     * @brief Integer type that can hold a page number in the MAGE-physical
     * address space.
     */
    using PhysPageNumber = std::uint64_t;

    /* INSTRUCTION NUMBERS */
    /**
     * @brief Integer type that can hold the index of an instruction in a
     * bytecode.
     */
    using InstructionNumber = std::uint64_t;

    /**
     * @brief Size of an instruction number, in bits.
     *
     * This should be set large enough to accommodate any reasonably-sized
     * bytecode (memory program).
     */
    const constexpr int instruction_number_bits = 48;

    /** @brief Sentinel value representing an invalid instruction number. */
    const constexpr std::uint64_t invalid_instr = (UINT64_C(1) << instruction_number_bits) - 1;

    /* STORAGE ADDRESSES */

    /**
     * @brief Integer type that can hold an an address (pointer) in the
     * storage (swap) address space.
     */
    using StorageAddr = std::uint64_t;

    /**
     * @brief Size of a storage (swap) address, in bits.
     *
     * This should be set large enough that the capacity of any
     * reasonably-sized swapfile or swap drive can be fully utilized.
     */
    const constexpr int storage_address_bits = 48;

    /**
     * @brief Sentinel value representing an invalid storage (swap) address.
     */
    const constexpr StorageAddr invalid_saddr = (UINT64_C(1) << storage_address_bits) - 1;

    /**
     * @brief Integer type that can hold a page number in the storage (swap)
     * address space.
     */
    using StoragePageNumber = std::uint64_t;

    /* CLUSTER */

    /**
     * @brief Integer type that can hold the ID of a worker in a
     * parallel/distributed MAGE program.
     */
    using WorkerID = std::uint32_t;

    /**
     * @brief Integer type that can hold the ID of a party in a MAGE program.
     *
     * In the case of Secure Multi-Party Computation, multiple parties may be
     * involved in a single MAGE computation.
     */
    using PartyID = std::uint32_t;

    /**
     * @brief Party ID used, by convention, for the evaluator party in
     * garbled-circuit-based secure computation.
     */
    constexpr const PartyID evaluator_party_id = 0;

    /**
     * @brief Party ID used, by convention, for the garbler party in
     * garbled-circuit-based secure computation.
     */
    constexpr const PartyID garbler_party_id = 1;
}

#endif
