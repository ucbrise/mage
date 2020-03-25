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

#ifndef MAGE_WIRE_HPP_
#define MAGE_WIRE_HPP_

#include <cassert>
#include <cstdint>

namespace mage {
    using WireGlobalID = std::int64_t;
    using WireLocalID = std::int64_t;

    /*
     * WireIDs are PODs that are small enough that they should just be passed
     * by value.
     */
    struct WireID {
    private:
        std::uint64_t data;

        std::int64_t index() const {
            return static_cast<std::int64_t>(this->data >> 1);
        }

    public:
        bool is_global() const {
            return (this->data & UINT64_C(0x1)) == 0x1;
        }

        void set_global(WireGlobalID id) {
            this->data = (static_cast<std::uint64_t>(id) << 1) | UINT64_C(0x1);
        }

        void set_local(WireLocalID id) {
            this->data = static_cast<std::uint64_t>(id) << 1;
        }

        WireGlobalID global_id() const {
            assert(this->is_global());
            return this->index();
        }

        WireLocalID local_id() const {
            assert(!this->is_global());
            return this->index();
        }
    };
}
#endif
