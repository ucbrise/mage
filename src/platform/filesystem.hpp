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
 * @file platform/filesystem.hpp
 * @brief System-level utilities for using the file system.
 *
 * Although POSIX APIs use off_t for file lengths and offsets, we use
 * std::uint64_t here. The reason is to keep these APIs platform-agnostic to
 * the extent possible, in case we decide to add support for Windows (for
 * example) at some point in the future.
 */

#ifndef MAGE_PLATFORM_FILESYSTEM_HPP_
#define MAGE_PLATFORM_FILESYSTEM_HPP_

#include <cstddef>
#include <cstdint>

namespace mage::platform {
    /**
     * @brief Creates and opens a file with the specified name and length.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param filename The name to give the created file.
     * @param length The length to which to initialize the created file, in
     * bytes.
     * @param direct If true, then a hint is given to the kernel to bypass the
     * the buffer caches when performing I/O operations using the returned file
     * descriptor.
     * @param unsparsify If true, then zero bytes are explicitly written to the
     * file up to the specified length, so that the file system allocates all
     * underlying space for the specified file length and the file is not
     * sparse.
     * @return A file descriptor for the created and opened file.
     */
    int create_file(const char* filename, std::uint64_t length, bool direct = false, bool unsparsify = false);

    /**
     * @brief Opens a file and (optionally) gets its length.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param filename The name of the file to open.
     * @param[out] length If it is not @p nullptr, it is populated with the
     * length of the file, in bytes.
     * @param direct If true, then a hint is given to the kernel to bypass the
     * the buffer caches when performing I/O operations using the returned file
     * descriptor.
     * @return A file descriptor for the opened file.
     */
    int open_file(const char* filename, std::uint64_t* length, bool direct = false);

    /**
     * @brief Obtains the length of the file corresponding to the specified
     * file descriptor.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param fd A file descriptor corresponding to the file whose length to
     * obtain.
     * @return The length, in bytes, of the file.
     */
    std::uint64_t length_file(int fd);

    /**
     * @brief Writes all of the provided data to the file associated with the
     * provided file descriptor, at the offset associated with the provided
     * file descriptor, and advances the file descriptor's offset accordingly.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param fd The provided file descriptor.
     * @param buffer A pointer to an array of data to write to the file.
     * @param length The length of the array, which is also the number of bytes
     * to write to the file.
     */
    void write_to_file(int fd, const void* buffer, std::size_t length);

    /**
     * @brief Writes all of the provided data to the file associated with the
     * provided file descriptor, at the specified offset. The offset associated
     * with the provided file descriptor is unchanged.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param fd The provided file descriptor.
     * @param buffer A pointer to an array of data to write to the file.
     * @param length The length of the array, which is also the number of bytes
     * to write to the file.
     * @param offset The offset in the file at which to write data.
     */
    void write_to_file_at(int fd, const void* buffer, std::size_t length, std::uint64_t offset);

    /**
     * @brief Reads the specified number of bytes from the file associated with
     * the provided file descriptor, at the offset associated with the provided
     * file descriptor, into the specified array, and advances the file
     * descriptor's offset accordingly.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param fd The provided file descriptor.
     * @param buffer The array into which to read data.
     * @param length The length of the array, which is also the number of bytes
     * to read from the file.
     * @return The number of bytes read from the file, which is different from
     * @p length if and only if the end-of-file condition is reached.
     */
    std::size_t read_from_file(int fd, void* buffer, std::size_t length);

    /**
     * @brief Reads the specified number of bytes from the file associated with
     * the provided file descriptor, at the specified offset. The offset
     * associated with the provided file descriptor is unchanged.
     *
     * @param fd The provided file descriptor.
     * @param buffer The array into which to read data.
     * @param length The length of the array, which is also the number of bytes
     * to read from the file.
     * @return The number of bytes read from the file, which is different from
     * @p length if and only if the end-of-file condition is reached.
     */
    std::size_t read_from_file_at(int fd, void* buffer, std::size_t length, std::uint64_t offset);

    /**
     * @brief Reads up to the specified number of bytes from the file
     * associated with the provided file descriptor, at the offset associated
     * with the provided file descriptor, into the specified array, and
     * advances the file descriptor's offset accordingly.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param fd The provided file descriptor.
     * @param buffer The array into which to read data.
     * @param length The length of the array, which is also the number of bytes
     * to read from the file.
     * @return The number of bytes read from the file, which may be less than
     * @p length for any reason, and is 0 if and only if the end-of-file
     * condition is reached.
     */
    std::size_t read_available_from_file(int fd, void* buffer, std::size_t length);

    /**
     * @brief Gives the kernel a hint to prefetch data at the speified position
     * in the file associated with the provided file descriptor.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param fd The provided file descriptor.
     * @param start The offset marking the beginning of the region of the file
     * to prefetch.
     * @param length The size, in bytes, of the region of the file to prefetch.
     */
    void prefetch_from_file_at(int fd, std::uint64_t start, std::size_t length);

    /**
     * @brief Changes the offset associated with a file descriptor that
     * corresponds to a file.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param fd The provided file descriptor.
     * @param amount The new offset.
     * @param relative If true, @p amount is treated as a relative offset with
     * respect to the file descriptor's current offset.
     */
    void seek_file(int fd, std::int64_t amount, bool relative = false);

    /**
     * @brief Obtains the offset associated with a file descriptor that
     * corresponds to a file.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param fd the provided file descriptor.
     * @return The offset associated with the provided file descriptor.
     */
    std::uint64_t tell_file(int fd);

    /**
     * @brief Closes a file descriptor. that corresponds to a file.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param fd The file descriptor to close.
     */
    void close_file(int fd);
}

#endif
