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

#ifndef MAGE_MEMPROG_PLACEMENT_HPP_
#define MAGE_MEMPROG_PLACEMENT_HPP_

#include <cassert>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "addr.hpp"
#include "util/prioqueue.hpp"

namespace mage::memprog {
    using AllocationSize = std::uint64_t;

    class Placer {
    public:
        virtual VirtAddr allocate_virtual(AllocationSize width, bool& fresh_page) = 0;
        virtual void deallocate_virtual(VirtAddr addr, AllocationSize width) = 0;
        virtual VirtPageNumber get_num_pages() const = 0;
    };

    class SimplePlacer {
    public:
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

    class FIFOPlacer {
    public:
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

    struct PageInfo {
        std::vector<VirtAddr> reusable_slots;
        std::uint64_t next_free_offset;
    };

    struct AllocationSizeInfo {
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

    class BinnedPlacer {
    public:
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
