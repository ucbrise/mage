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
 * @file memprog/placement.hpp
 * @brief Placement stage for MAGE's planner
 *
 * The placement module is, in effect, a memory allocator for the MAGE-virtual
 * address space.
 */

#ifndef MAGE_MEMPROG_PLACEMENT_HPP_
#define MAGE_MEMPROG_PLACEMENT_HPP_

#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "addr.hpp"
#include "util/prioqueue.hpp"

namespace mage::memprog {
    /**
     * @brief Integer type that can hold the size of a variable to place.
     */
    using AllocationSize = std::uint64_t;

    /**
     * @brief Enumeration type that describes the type of a variable to place.
     */
    enum class PlaceableType : std::uint64_t {
        Ciphertext = 0,
        Plaintext = 1,
        DenormalizedCiphertext = 2,
    };

    /**
     * @brief Obtains a human-readable name for the type of a variable to
     * place.
     *
     * @param p The type of the variable to place.
     * @return The human-readable name for the provided type.
     */
    constexpr const char* placeable_type_name(PlaceableType p) {
        switch (p) {
        case PlaceableType::Ciphertext:
            return "Ciphertext";
        case PlaceableType::Plaintext:
            return "Plaintext";
        case PlaceableType::DenormalizedCiphertext:
            return "DenormalizedCiphertext";
        default:
            return "INVALID";
        }
    }

    /**
     * @brief Type for a function that specifies the size for placement of each
     * variable type (i.e., the space such a variable would occupy in the
     * MAGE-virtual address space) for a target protocol.
     */
    using PlacementPlugin = std::function<AllocationSize(std::uint64_t, PlaceableType)>;

    /**
     * @brief Exception type that indicates that placement of a variable was
     * attempted, but it could not be completed with the current configuration.
     */
    class InvalidPlacementException : public std::runtime_error {
    public:
        /**
         * @brief Creates an @p InvalidPlacementException object for the
         * specified porotocol, allocation width, and variable type.
         *
         * @param protocol The specified protocol.
         * @param logical_width The specified allocation width.
         * @param type The specified variable type.
         */
        InvalidPlacementException(const std::string& protocol, std::uint64_t logical_width, PlaceableType type)
            : std::runtime_error("Invalid placement for protocol \"" + protocol + "\": logical width = " + std::to_string(logical_width) + ", type = " + placeable_type_name(type)) {
        }
    };

    /**
     * @brief Abstract class for a Placement module in MAGE's planner.
     */
    class Placer {
    public:
        /**
         * @brief Places a variable in the MAGE-virtual addres space.
         *
         * @param width The width of the variable to place (i.e., the amount of
         * MAGE-virtual memory to allocate).
         * @param[out] fresh_page Set to true if the variable is placed on a
         * page on which no other variables have been placed.
         * @return The MAGE-virtual address at which the variable is placed.
         */
        virtual VirtAddr allocate_virtual(AllocationSize width, bool& fresh_page) = 0;

        /**
         * @brief Deallocates space for a previously-allocated variable,
         * usually called when a variable goes out of scope.
         *
         * @param addr The address of the memory to deallocate.
         * @param width The size of the memory to deallocate, in the
         * MAGE-virtual address space.
         */
        virtual void deallocate_virtual(VirtAddr addr, AllocationSize width) = 0;

        /**
         * @brief Returns the number of pages used in the MAGE-virtual address
         * space.
         *
         * @return The number of pages used in the MAGE-virtual address space.
         */
        virtual VirtPageNumber get_num_pages() const = 0;
    };

    /**
     * @brief Simple baseline placement module that never deallocates memory.
     */
    class SimplePlacer {
    public:
        /**
         * @brief Creates a @p SimplePlacer with the specified page size.
         *
         * @param shift Base-2 logarithm of the page size.
         */
        SimplePlacer(PageShift shift) : next_free_address(0), page_shift(shift) {
        }

        VirtAddr allocate_virtual(AllocationSize width, bool& fresh_page) {
            VirtAddr addr;
            assert(width != 0);
            if (pg_num(this->next_free_address, this->page_shift) == pg_num(this->next_free_address + width - 1, this->page_shift)) {
                addr = this->next_free_address;
            } else {
                addr = pg_next(this->next_free_address, this->page_shift);
            }
            this->next_free_address = addr + width;
            fresh_page = (pg_offset(addr, this->page_shift) == 0);
            return addr;
        }

        void deallocate_virtual(VirtAddr addr, AllocationSize width) {
            // No-op: don't reclaim free space
        }

        VirtPageNumber get_num_pages() const {
            VirtPageNumber num_pages = pg_num(this->next_free_address, this->page_shift);
            if (pg_offset(this->next_free_address, this->page_shift) == 0) {
                num_pages--;
            }
            return num_pages;
        }

    private:
        VirtAddr next_free_address;
        PageShift page_shift;
    };

    /**
     * @brief Simple placement policy that places equal-width items on the same
     * page and recycles slots using a FIFO policy.
     */
    class FIFOPlacer {
    public:
        /**
         * @brief Creates a @p FIFOPlacer with the specified page size.
         *
         * @param shift Base-2 logarithm of the page size.
         */
        FIFOPlacer(PageShift shift) : next_page(0), page_shift(shift) {
        }

        VirtAddr allocate_virtual(AllocationSize width, bool& fresh_page) {
            std::vector<VirtAddr>& free_slots = this->slot_map[width];

            VirtAddr result;
            if (free_slots.empty()) {
                VirtPageNumber page = this->next_page++;
                VirtAddr page_addr = pg_addr(page, this->page_shift);
                std::uint64_t offset;
                for (offset = pg_size(this->page_shift) - width; offset >= width; offset -= width) {
                    free_slots.push_back(page_addr + offset);
                }
                result = page_addr + offset;
                fresh_page = true;
            } else {
                result = free_slots.back();
                free_slots.pop_back();
                fresh_page = false;
            }

            assert(!this->allocated.contains(result));
            this->allocated.insert(result);

            return result;
        }

        void deallocate_virtual(VirtAddr addr, AllocationSize width) {
            assert(this->allocated.contains(addr));
            this->allocated.erase(addr);
            this->slot_map[width].push_back(addr);
        }

        VirtPageNumber get_num_pages() const {
            return this->next_page;
        }

    private:
        std::unordered_map<AllocationSize, std::vector<VirtAddr>> slot_map;
        std::unordered_set<VirtAddr> allocated;
        VirtPageNumber next_page;
        PageShift page_shift;
    };

    /**
     * @brief Stores which slots are free in a given MAGE-virtual page, and
     * which slots have been allocated before.
     */
    struct PageInfo {
        std::vector<VirtAddr> reusable_slots;
        std::uint64_t next_free_offset;
    };

    /**
     * @brief Stores information for allocations of a particular size (used in
     * the @p BinnedPlacer).
     *
     * Keeps track of all pages for allocations of a particular size, including
     * information on how close to full each page is and the location of free
     * slots within each page.
     */
    struct AllocationSizeInfo {
        /**
         * @brief Creates an @p AllocationSizeInfo objct for allocations of the
         * specified size.
         *
         * If the allocation size is too large for the given page size, then
         * the process is aborted.
         *
         * @param shift Base-2 logarithm of the page size.
         * @param width The specified allocation size.
         */
        AllocationSizeInfo(PageShift shift, AllocationSize width) {
            this->fresh_page_free_slots = pg_size(shift) / width;
            if (this->fresh_page_free_slots == 0) {
                std::cerr << "Page size must be greater than largest allocation size" << std::endl;
                std::abort();
            }
        }
        util::PriorityQueue<std::uint64_t, VirtPageNumber> unfilled_pages;
        std::unordered_map<VirtPageNumber, PageInfo> page_info;
        std::uint64_t fresh_page_free_slots;
    };

    /**
     * @brief The placement module used by MAGE's default planning pipeline.
     *
     * In addition to the equal-width heuristic used by the @p FIFOPlacer, it
     * aims to reduce fragmentation by trying to avoid keeping pages only
     * partially filled.
     */
    class BinnedPlacer {
    public:
        /**
         * @brief Creates a @p BinnedPlacer with the specified page size.
         *
         * @param shift Base-2 logarithm of the page size.
         */
        BinnedPlacer(PageShift shift) : next_page(0), page_shift(shift) {
        }

        VirtAddr allocate_virtual(AllocationSize width, bool& fresh_page) {
            AllocationSizeInfo& bwi = this->get_info(width);

            VirtAddr result;
            if (bwi.unfilled_pages.empty()) {
                VirtPageNumber page = this->next_page++;
                VirtAddr page_addr = pg_addr(page, this->page_shift);

                PageInfo& page_info = bwi.page_info[page];
                page_info.next_free_offset = width;
                result = page_addr;
                fresh_page = true;

                // std::vector<VirtAddr>& free_slots = bwi.free_slots_by_page[page];
                // free_slots.reserve(pg_size(this->page_shift) / width);
                // std::uint64_t offset;
                // for (offset = pg_size(this->page_shift) - width; offset >= width; offset -= width) {
                //     free_slots.push_back(page_addr + offset);
                // }
                // result = page_addr + offset;
                // fresh_page = true;

                std::uint64_t num_free_slots = (pg_size(this->page_shift) - page_info.next_free_offset) / width;
                if (num_free_slots > 0) {
                    bwi.unfilled_pages.insert(num_free_slots, page);
                }
            } else {
                std::pair<std::uint64_t, VirtPageNumber>& rv = bwi.unfilled_pages.min();
                VirtPageNumber page = rv.second;
                std::uint64_t num_free_slots = rv.first;
                PageInfo& page_info = bwi.page_info[page];
                if (page_info.reusable_slots.size() > 0) {
                    result = page_info.reusable_slots.back();
                    page_info.reusable_slots.pop_back();
                } else {
                    result = pg_addr(page, this->page_shift) + page_info.next_free_offset;
                    page_info.next_free_offset += width;
                    assert(pg_size(this->page_shift) >= page_info.next_free_offset);
                }
                // std::vector<VirtAddr>& free_slots = bwi.free_slots_by_page[page];
                // result = free_slots.back();
                // free_slots.pop_back();
                fresh_page = false;

                if (num_free_slots == 1) {
                    bwi.unfilled_pages.remove_min();
                } else {
                    bwi.unfilled_pages.decrease_key(num_free_slots - 1, page);
                }
            }

            return result;
        }

        void deallocate_virtual(VirtAddr addr, AllocationSize width) {
            AllocationSizeInfo& bwi = this->get_info(width);
            VirtPageNumber page = pg_num(addr, this->page_shift);

            std::uint64_t num_free_slots;
            if (!bwi.unfilled_pages.contains(page)) {
                num_free_slots = 1;
                if (num_free_slots == bwi.fresh_page_free_slots && bwi.unfilled_pages.size() > 0) {
                    bwi.page_info.erase(page);
                } else {
                    bwi.unfilled_pages.insert(num_free_slots, page);
                    bwi.page_info[page].reusable_slots.push_back(addr);
                }
                return;
            }

            num_free_slots = bwi.unfilled_pages.get_key(page);
            num_free_slots++;
            /*
             * Both here and above, the criterion for deciding when to remove
             * the entry for the page is that (1) it is completely empty, and
             * (2) there will remain an entry for at least one unfilled page
             * once we remove it. The second part is an optimization, for cases
             * where an element of a particular size is repeatedly allocated
             * and deallocated. For example, in merge_sorted, I've observed the
             * number of allocated Integer<1> objects oscillate between 0 and
             * 1, which would cause an entirely new page entry to be
             * initialized and freed on every allocation. I've observed that
             * adding condition (2) improved placement performance by 10-20%.
             * But it could potentially result in wasted memory when planning
             * if many item widths have this extra page entry. For the programs
             * I've seen, there are very few different object widths allocated,
             * so this is not a problem. (There are other problems if we have a
             * large number of widths, since objects of different widths cannot
             * share a page). I'm putting in this note in case this extra
             * memory used during placement becomes important later.
             * If it's ever necessary to revert to condition (1) only, the
             * correct "if" predicate, both here and above, is
             * num_free_slots == bwi.fresh_page_free_slots.
             */
            if (num_free_slots == bwi.fresh_page_free_slots && bwi.unfilled_pages.size() > 1) {
                bwi.unfilled_pages.erase(page);
                bwi.page_info.erase(page);
            } else {
                bwi.unfilled_pages.increase_key(num_free_slots, page);
                bwi.page_info[page].reusable_slots.push_back(addr);
            }
        }

        VirtPageNumber get_num_pages() const {
            return this->next_page;
        }

    private:
        /**
         * @brief Obtains a reference to the @p AllocationSizeInfo object for a
         * given allocation size, creating the @p AllocationSizeInfo object for
         * that size if it does not yet exist.
         */
        AllocationSizeInfo& get_info(AllocationSize width) {
            auto iter = this->slot_map.find(width);
            if (iter != this->slot_map.end()) {
                return iter->second;
            }
            auto p = this->slot_map.try_emplace(width, this->page_shift, width);
            return p.first->second;
        }

        std::unordered_map<AllocationSize, AllocationSizeInfo> slot_map;
        VirtPageNumber next_page;
        PageShift page_shift;
    };
}

#endif
