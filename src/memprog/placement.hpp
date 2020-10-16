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
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "addr.hpp"
#include "util/prioqueue.hpp"

namespace mage::memprog {
    class Placer {
    public:
        virtual VirtAddr allocate_virtual(BitWidth width, bool& fresh_page) = 0;
        virtual void deallocate_virtual(VirtAddr addr, BitWidth width) = 0;
        virtual VirtPageNumber get_num_pages() const = 0;
    };

    class SimplePlacer {
    public:
        SimplePlacer(PageShift shift) : next_free_address(0), page_shift(shift) {
        }

        VirtAddr allocate_virtual(BitWidth width, bool& fresh_page) {
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

        void deallocate_virtual(VirtAddr addr, BitWidth width) {
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

    struct BitWidthInfo {
        BitWidthInfo(PageShift shift, BitWidth width) {
            this->fresh_page_free_slots = pg_size(shift) / width;
        }
        util::PriorityQueue<std::uint64_t, VirtPageNumber> unfilled_pages;
        std::unordered_map<VirtPageNumber, std::vector<VirtAddr>> free_slots_by_page;
        std::uint64_t fresh_page_free_slots;
    };

    class FIFOPlacer {
    public:
        FIFOPlacer(PageShift shift) : next_page(0), page_shift(shift) {
        }

        VirtAddr allocate_virtual(BitWidth width, bool& fresh_page) {
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

        void deallocate_virtual(VirtAddr addr, BitWidth width) {
            assert(this->allocated.contains(addr));
            this->allocated.erase(addr);
            this->slot_map[width].push_back(addr);
        }

        VirtPageNumber get_num_pages() const {
            return this->next_page;
        }

    private:
        std::unordered_map<BitWidth, std::vector<VirtAddr>> slot_map;
        std::unordered_set<VirtAddr> allocated;
        VirtPageNumber next_page;
        PageShift page_shift;
    };

    class BinnedPlacer {
    public:
        BinnedPlacer(PageShift shift) : next_page(0), page_shift(shift) {
        }

        VirtAddr allocate_virtual(BitWidth width, bool& fresh_page) {
            BitWidthInfo& bwi = this->get_info(width);

            VirtAddr result;
            if (bwi.unfilled_pages.empty()) {
                VirtPageNumber page = this->next_page++;
                VirtAddr page_addr = pg_addr(page, this->page_shift);

                std::vector<VirtAddr>& free_slots = bwi.free_slots_by_page[page];
                std::uint64_t offset;
                for (offset = pg_size(this->page_shift) - width; offset >= width; offset -= width) {
                    free_slots.push_back(page_addr + offset);
                }
                result = page_addr + offset;
                fresh_page = true;

                if (free_slots.size() != 0) {
                    bwi.unfilled_pages.insert(free_slots.size(), page);
                }
            } else {
                std::pair<std::uint64_t, VirtPageNumber>& rv = bwi.unfilled_pages.min();
                VirtPageNumber page = rv.second;
                std::uint64_t num_free_slots = rv.first;
                std::vector<VirtAddr>& free_slots = bwi.free_slots_by_page[page];
                result = free_slots.back();
                free_slots.pop_back();
                fresh_page = false;

                if (num_free_slots == 1) {
                    bwi.unfilled_pages.remove_min();
                } else {
                    bwi.unfilled_pages.decrease_key(num_free_slots - 1, page);
                }
            }

            return result;
        }

        void deallocate_virtual(VirtAddr addr, BitWidth width) {
            BitWidthInfo& bwi = this->get_info(width);
            VirtPageNumber page = pg_num(addr, this->page_shift);

            std::uint64_t num_free_slots;
            if (!bwi.unfilled_pages.contains(page)) {
                num_free_slots = 1;
                if (num_free_slots == bwi.fresh_page_free_slots) {
                    bwi.free_slots_by_page.erase(page);
                } else {
                    bwi.unfilled_pages.insert(num_free_slots, page);
                    bwi.free_slots_by_page[page].push_back(addr);
                }
                return;
            }

            num_free_slots = bwi.unfilled_pages.get_key(page);
            num_free_slots++;
            if (num_free_slots == pg_size(this->page_shift) / width) {
                bwi.unfilled_pages.erase(page);
                bwi.free_slots_by_page.erase(page);
            } else {
                bwi.unfilled_pages.increase_key(num_free_slots, page);
                bwi.free_slots_by_page[page].push_back(addr);
            }
        }

        VirtPageNumber get_num_pages() const {
            return this->next_page;
        }

    private:
        BitWidthInfo& get_info(BitWidth width) {
            auto iter = this->slot_map.find(width);
            if (iter != this->slot_map.end()) {
                return iter->second;
            }
            auto p = this->slot_map.try_emplace(width, this->page_shift, width);
            return p.first->second;
        }

        std::unordered_map<BitWidth, BitWidthInfo> slot_map;
        VirtPageNumber next_page;
        PageShift page_shift;
    };
}

#endif
