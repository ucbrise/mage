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

#ifndef MAGE_UTIL_MAPPING_HPP_
#define MAGE_UTIL_MAPPING_HPP_

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include "platform/memory.hpp"

namespace mage::util {
    template <typename T>
    const T* reverse_list_into(const T* from, void* into, std::size_t num_bytes) {
        std::uint8_t* target = reinterpret_cast<std::uint8_t*>(into) + num_bytes;
        while (target != into) {
            std::size_t size = from->size();
            const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(from);
            target -= size;
            std::copy(ptr, ptr + size, target);
            from = from->next();
        }
        return from;
    }

    template <typename T>
    void reverse_list_into(const T* from, const char* to, std::size_t num_bytes) {
        platform::MappedFile<T> to_file(to, num_bytes);
        reverse_list_into(from, to_file.mapping(), num_bytes);
    }

    template <typename T>
    void reverse_list(const T* from, std::size_t num_bytes) {
        platform::MappedFile<std::uint8_t> anonymous(num_bytes, true);
        std::uint8_t* scratch = anonymous.mapping();
        reverse_list_into(from, scratch, num_bytes);
        std::copy(scratch, scratch + num_bytes, reinterpret_cast<std::uint8_t*>(from));
    }

    template <typename T>
    void reverse_file(const char* filename) {
        platform::MappedFile<T> file(filename, true);
        reverse_list(file.mapping(), file.size());
    }

    template <typename T>
    void reverse_file_into(const char* from, const char* to) {
        platform::MappedFile<T> from_file(from, false);
        platform::MappedFile<T> to_file(to, from_file.size());
        reverse_list_into(from_file.mapping(), to_file.mapping(), to_file.size());
    }
}

#endif
