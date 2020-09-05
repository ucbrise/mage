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

#ifndef MAGE_PLATFORM_NETWORK_HPP_
#define MAGE_PLATFORM_NETWORK_HPP_

#include <cstddef>
#include <cstdint>

namespace mage::platform {
    void network_accept(const char* port, int* into, std::uint32_t count = 1);
    void network_connect(const char* host, const char* port, int* into, std::uint32_t count = 1);
    void network_close(int socket);

    void pipe_open(int* into);
    void pipe_close(int fd);
}

#endif
