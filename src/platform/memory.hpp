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

/**
 * @file platform/memory.hpp
 * @brief System-level utilities for memory management.
 */

#ifndef MAGE_PLATFORM_MEMORY_HPP_
#define MAGE_PLATFORM_MEMORY_HPP_

#include <cstddef>

#include "platform/filesystem.hpp"

namespace mage::platform {
    /**
     * @brief Allocates memory directly from the operating system.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param num_bytes The number of bytes to allocate, which may be rounded
     * up to the nearest multiple of the page size.
     * @param lazy If true, the memory may be allocated more quickly, but minor
     * page faults may occur on the first access to each page in the allocated
     * memory.
     * @return A pointer to the allocated memory, which is guaranteed to be
     * page-aligned.
     */
    void* allocate_resident_memory(std::size_t num_bytes, bool lazy = false);

    /**
     * @brief Deallocates memory directly to the operating system.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param memory A pointer to the memory to deallocate, which must be
     * page-aligned.
     * @param num_bytes The number of bytes to deallocate, which must be a
     * multiple of the page size.
     */
    void deallocate_resident_memory(void* memory, std::size_t num_bytes);

    /**
     * @brief Allocates memory directly from the operating system, returning
     * a pointer of the specified type.
     *
     * If an error occurs, then the process is aborted.
     *
     * @tparam T The type to whose pointer to cast the resulting pointer to the
     * allocated memory.
     * @param num_bytes The number of bytes to allocate, which may be rounded
     * up to the nearest multiple of the page size.
     * @param lazy If true, the memory may be allocated more quickly, but minor
     * page faults may occur on the first access to each page in the allocated
     * memory.
     * @return A pointer to the allocated memory, which is guaranteed to be
     * page-aligned.
     */
    template <typename T>
    T* allocate_resident_memory(std::size_t num_bytes, bool lazy = false) {
        void* memory = allocate_resident_memory(num_bytes, lazy);
        return reinterpret_cast<T*>(memory);
    }

    /**
     * @brief Maps a file into memory.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param fd A file descriptor for the file to map.
     * @param length The size of the mapping, in bytes.
     * @param mutate If true, modifications to the memory mapping are
     * propagated to the underlying file. If false, they are not.
     * @return A pointer to the memory mapping, which is guaranteed to be
     * page-aligned.
     */
    void* map_file(int fd, std::size_t length, bool mutate = true);

    /**
     * @brief Unmaps all or part of a file-backed memory mapping.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param memory A pointer to the memory to unmap, which must be
     * page-aligned.
     * @param num_bytes The number of bytes to unmap, which must be a
     * multiple of the page size.
     */
    void unmap_file(void* memory, std::size_t length);

    /**
     * @brief Maps a file into memory, returning a pointer of the specified
     * type.
     *
     * If an error occurs, then the process is aborted.
     *
     * @tparam T The type to whose pointer to cast the resulting pointer to the
     * memory mapping.
     * @param fd A file descriptor for the file to map.
     * @param length The size of the mapping, in bytes.
     * @param mutate If true, modifications to the memory mapping are
     * propagated to the underlying file. If false, they are not.
     * @return A pointer to the memory mapping, which is guaranteed to be
     * page-aligned.
     */
    template <typename T>
    T* map_file(int fd, std::size_t length, bool mutate = true) {
        void* memory = map_file(fd, length, mutate);
        return reinterpret_cast<T*>(memory);
    }

    /**
     * @brief RAII-style wrapper for a memory mapping or allocated memory.
     *
     * @tparam T The type to whose pointer to cast the pointer to the
     * underlying memory.
     */
    template <typename T>
    class MappedFile {
    public:
        /**
         * @brief Open the file with the provided file name, map it into
         * memory, and wrap the memory mapping in the created @p MappedFile
         * object.
         *
         * If an error occurs, then the process is aborted.
         *
         * @param filename The name of the file to open and map into memory.
         * @param mutate If true, modifications to the memory mapping are
         * propagated to the underlying file. If false, they are not.
         */
        MappedFile(const char* filename, bool mutate = true) {
            int fd = open_file(filename, &this->length);
            this->data = map_file<T>(fd, this->length, mutate);
            close_file(fd);
        }

        /**
         * @brief Create a new file, initialize it to the specified length,
         * map it into memory, and wrap the memory mapping in the created
         * @p MappedFile object.
         *
         * If an error occurs, then the process is aborted.
         *
         * @param filename The name to give the created file.
         * @param num_bytes The length, in bytes, to which to initialize the
         * file.
         */
        MappedFile(const char* filename, std::size_t num_bytes) {
            int fd = create_file(filename, num_bytes);
            this->data = map_file<T>(fd, num_bytes);
            close_file(fd);
            this->length = num_bytes;
        }

        /**
         * @brief Allocates memory directly from the operating system and wraps
         * it in the created @p MappedFile object.
         *
         * If an error occurs, then the process is aborted.
         *
         * @param num_bytes The size, in bytes, of the memory allocation.
         * @param lazy If true, the memory may be allocated more quickly, but minor
         * page faults may occur on the first access to each page in the allocated
         * memory.
         */
        MappedFile(std::size_t num_bytes, bool lazy) {
            this->data = allocate_resident_memory<T>(num_bytes, lazy);
            this->length = num_bytes;
        }

        /**
         * @brief Move constructor.
         *
         * @param other the @p MappedFile object from which to construct this
         * one.
         */
        MappedFile(MappedFile<T>&& other) {
            this->data = other.data;
            this->length = other.length;
            other.data = nullptr;
            other.length = 0;
        }

        /**
         * @brief Destructor.
         */
        ~MappedFile() {
            if (this->length != 0) {
                unmap_file(this->data, this->length);
            }
        }

        /**
         * @brief Returns a pointer to the underlying memory mapping or
         * allocated memory.
         *
         * @return A pointer to the underlying memory wrapped by this
         * @p MappedFile object.
         */
        T* mapping() const {
            return this->data;
        }

        /**
         * @brief Returns the size of the underlying memory mapping or
         * allocated memory.
         *
         * @return The size of the underlying memory wrapped by this
         * @p MappedFile object.
         */
        std::size_t size() const {
            return this->length;
        }

    private:
        T* data;
        std::size_t length;
    };
}

#endif
