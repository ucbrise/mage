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

#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <sys/mman.h>

namespace mage::platform {
    void* allocate_resident_memory(std::size_t numbytes) {
        void* region = mmap(NULL, numbytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_POPULATE, -1, 0);
        if (region == MAP_FAILED) {
            std::perror("allocate_resident_memory -> mmap");
            std::abort();
        }
        return region;
    }

    void deallocate_resident_memory(void* memory, std::size_t numbytes) {
        if (munmap(memory, numbytes) != 0) {
            std::perror("deallocate_resident_memory -> munmap");
            std::abort();
        }
    }

    void* map_file(int fd, std::size_t length) {
        void* region = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (region == MAP_FAILED) {
            std::perror("map_file -> mmap");
            std::abort();
        }
        return region;
    }

    void unmap_file(void* memory, std::size_t length) {
        if (munmap(memory, length) != 0) {
            std::perror("unmap_file -> munmap");
            std::abort();
        }
    }
}
