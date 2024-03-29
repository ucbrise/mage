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

#include "platform/misc.hpp"
#include <cstdint>
#include <sys/ioctl.h>
#include <unistd.h>

namespace mage::platform {
    void get_terminal_size(TerminalSize& ts) {
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
            ts.num_rows = ws.ws_row;
            ts.num_cols = ws.ws_col;
        } else {
            ts.num_rows = 0;
            ts.num_cols = 0;
        }
    }
}
