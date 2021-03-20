/*
 * Copyright (C) 2021 Sam Kumar <samkumar@cs.berkeley.edu>
 * Copyright (C) 2021 University of California, Berkeley
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
 * @file platform/misc.hpp
 * @brief Miscellaneous system-level utilities.
 */

#include <cstdint>

namespace mage::platform {
    /**
     * @brief Describes the size of the terminal window.
     */
    struct TerminalSize {
        std::uint32_t num_rows;
        std::uint32_t num_cols;
    };

    /**
     * @brief Populates @p tz with the terminal window size.
     *
     * If the terminal size could not be obtained (e.g., because the system
     * does not support it, because standard output is not connected to a
     * terminal window, or the operation is unsupported) then @p tz is
     * populated with zero rows and zero columns.
     *
     * @param[out] tz The structure to populate with the terminal window size.
     */
    void get_terminal_size(TerminalSize& ts);
}
