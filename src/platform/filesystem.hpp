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

#ifndef MAGE_PLATFORM_FILESYSTEM_HPP_
#define MAGE_PLATFORM_FILESYSTEM_HPP_

#include <cstddef>
#include <cstdint>

namespace mage::platform {
    /* I'd prefer off_t instead of std::uint64_t, but off_t is POSIX. */
    int create_file(const char* filename, std::uint64_t length, bool direct = false, bool unsparsify = false);
    int open_file(const char* filename, std::uint64_t* length, bool direct = false);
    std::uint64_t length_file(int fd);
    void write_to_file(int fd, const void* buffer, std::size_t length);
    void write_to_file_at(int fd, const void* buffer, std::size_t length, std::uint64_t offset);
    std::size_t read_from_file(int fd, void* buffer, std::size_t length);
    std::size_t read_from_file_at(int fd, void* buffer, std::size_t length, std::uint64_t offset);
    std::size_t read_available_from_file(int fd, void* buffer, std::size_t length);
    void seek_file(int fd, std::int64_t amount, bool relative = false);
    std::uint64_t tell_file(int fd);
    void close_file(int fd);
}

#endif
