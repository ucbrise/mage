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

#ifndef MAGE_LOADER_LOADER_HPP_
#define MAGE_LOADER_LOADER_HPP_

#include "../gate.hpp"

namespace mage {
    /* Defined in scheduler.hpp */
    class Scheduler;
}

namespace mage::loader {
    class GatePageLoader {
    public:
        virtual void gate_page_executed(GatePageID id, GatePage* page) = 0;
        virtual void start_loading(Scheduler* s) = 0;
    };
}

#endif
