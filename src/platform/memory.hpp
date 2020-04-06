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

#ifndef MAGE_PLATFORM_MEMORY_HPP_
#define MAGE_PLATFORM_MEMORY_HPP_

#include <cstddef>

#include "platform/filesystem.hpp"

namespace mage::platform {
    void* allocate_resident_memory(std::size_t numbytes, bool swappable = false);
    void deallocate_resident_memory(void* memory, std::size_t numbytes);

    template <typename T>
    T* allocate_resident_memory(std::size_t numbytes, bool swappable = false) {
        void* memory = allocate_resident_memory(numbytes, swappable);
        return reinterpret_cast<T*>(memory);
    }

    void* map_file(int fd, std::size_t length, bool mutate = true);
    void unmap_file(void* memory, std::size_t length);

    template <typename T>
    T* map_file(int fd, std::size_t length, bool mutate = true) {
        void* memory = map_file(fd, length, mutate);
        return reinterpret_cast<T*>(memory);
    }

    template <typename T>
    class MappedFile {
    public:
        MappedFile(const char* filename, bool mutate = true) {
            int fd = open_file(filename, &this->length);
            this->data = map_file<T>(fd, this->length, mutate);
            close_file(fd);
        }

        MappedFile(const char* filename, std::size_t length) {
            int fd = create_file(filename, length);
            this->data = map_file<T>(fd, length);
            close_file(fd);
            this->length = length;
        }

        MappedFile(MappedFile<T>&& other) {
            this->data = other.data;
            this->length = other.length;
            other.data = nullptr;
            other.length = 0;
        }

        ~MappedFile() {
            if (this->length != 0) {
                unmap_file(this->data, this->length);
            }
        }

        T* mapping() const {
            return this->data;
        }

        std::size_t size() const {
            return this->length;
        }

    private:
        T* data;
        std::size_t length;
    };
}

#endif
