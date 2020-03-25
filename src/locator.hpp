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

#ifndef MAGE_LOCATOR_HPP_
#define MAGE_LOCATOR_HPP_

#include <cassert>
#include <cstdint>
#include "wire.hpp"
#include "platform/memory.hpp"

namespace mage {
    /* Locator finds wire data in memory to execute a gate. */
    template <typename Wire>
    class Locator {
        virtual Wire* acquire(WireID w) = 0;
        virtual void release(WireID w) = 0;
    };

    /* WireFitLocator assumes all wires fit in memory. */
    template <typename Wire>
    class WireFitLocator : Locator<Wire> {
        Wire* wires;
        std::uint64_t numwires;

    public:
        WireFitLocator(std::uint64_t numwires) {
            this->numwires = numwires;
            this->wires = platform::allocate_resident_memory(numwires * sizeof(Wire));
        }

        ~WireFitLocator() {
            platform::deallocate_resident_memory(this->wires, this->numwires * sizeof(Wire));
        }

        virtual Wire* acquire(WireID w) {
            assert(w.global_id() < this->numwires);
            return &this->wires[w.global_id()];
        }

        virtual void release(WireID w) {
            // Do nothing
        }
    };
}

#endif
