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
#include <cstdint>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include "memory.hpp"

namespace mage::platform {
    int create_file(const char* filename, std::uint64_t length, bool direct, bool unsparsify) {
        int flags = O_CREAT | O_RDWR | O_TRUNC;
        if (direct) {
            flags |= O_DIRECT;
        }
        int fd = open(filename, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fd == -1) {
            std::perror("create_file -> open");
            std::abort();
        }
        if (ftruncate(fd, (off_t) length) != 0) {
            std::perror("create_file -> ftruncate");
            std::abort();
        }
        if (unsparsify) {
            static constexpr const std::uint64_t buf_size = 4096;
            std::uint8_t* buf = allocate_resident_memory<std::uint8_t>(buf_size);
            std::fill(buf, buf + buf_size, 0x00);
            std::uint64_t left = length;
            while (left != 0) {
                ssize_t rv = write(fd, buf, std::min(left, buf_size));
                if (rv < 0) {
                    std::perror("create_file -> write");
                    std::abort();
                }
                left -= rv;
            }
            deallocate_resident_memory(buf, buf_size);
        }
        return fd;
    }

    int open_file(const char* filename, std::uint64_t* length, bool direct) {
        int flags = O_RDWR;
        if (direct) {
            flags |= O_DIRECT;
        }
        int fd = open(filename, flags);
        if (fd == -1) {
            std::perror("open_file -> open");
            std::abort();
        }
        if (length != nullptr) {
            off_t end = lseek(fd, 0, SEEK_END);
            if (end == (off_t) -1) {
                std::perror("open_file -> lseek");
                std::abort();
            }
            *length = (std::uint64_t) end;
        }
        return fd;
    }

    std::uint64_t length_file(int fd) {
        off_t pos = lseek(fd, 0, SEEK_CUR);
        if (pos == (off_t) -1) {
            std::perror("length_file -> lseek");
            std::abort();
        }
        off_t end = lseek(fd, 0, SEEK_END);
        if (end == (off_t) -1) {
            std::perror("length_file -> lseek");
            std::abort();
        }
        off_t rv = lseek(fd, pos, SEEK_SET);
        if (rv == (off_t) -1) {
            std::perror("length_file -> lseek");
            std::abort();
        }
        return end;
    }

    void write_to_file(int fd, const void* buffer, std::size_t length) {
        const std::uint8_t* data = reinterpret_cast<const std::uint8_t*>(buffer);
        std::size_t processed = 0;
        while (processed != length) {
            ssize_t rv = write(fd, &data[processed], length - processed);
            if (rv <= 0) {
                if (rv < 0) {
                    std::perror("write_to_file -> write");
                }
                std::abort();
            }
            processed += rv;
        }
    }

    void write_to_file_at(int fd, const void* buffer, std::size_t length, std::uint64_t offset) {
        const std::uint8_t* data = reinterpret_cast<const std::uint8_t*>(buffer);
        std::size_t processed = 0;
        while (processed != length) {
            ssize_t rv = pwrite(fd, &data[processed], length - processed, (off_t) (offset + processed));
            if (rv <= 0) {
                if (rv < 0) {
                    std::perror("write_to_file_at -> pwrite");
                }
                std::abort();
            }
            processed += rv;
        }
    }

    std::size_t read_from_file(int fd, void* buffer, std::size_t length) {
        std::uint8_t* data = reinterpret_cast<std::uint8_t*>(buffer);
        std::size_t processed = 0;
        while (processed != length) {
            ssize_t rv = read(fd, &data[processed], length - processed);
            if (rv <= 0) {
                if (rv < 0) {
                    std::perror("read_from_file -> read");
                    std::abort();
                }
                break;
            }
            processed += rv;
        }
        return processed;
    }

    std::size_t read_from_file_at(int fd, void* buffer, std::size_t length, std::uint64_t offset) {
        std::uint8_t* data = reinterpret_cast<std::uint8_t*>(buffer);
        std::size_t processed = 0;
        while (processed != length) {
            ssize_t rv = pread(fd, &data[processed], length - processed, (off_t) (offset + processed));
            if (rv <= 0) {
                if (rv < 0) {
                    std::perror("read_from_file -> pread");
                    std::abort();
                }
                break;
            }
            processed += rv;
        }
        return processed;
    }

    std::size_t read_available_from_file(int fd, void* buffer, std::size_t length) {
        std::uint8_t* data = reinterpret_cast<std::uint8_t*>(buffer);
        ssize_t rv = read(fd, data, length);
        if (rv < 0) {
            std::perror("read_from_file -> read");
            std::abort();
        }
        return rv;
    }

    void seek_file(int fd, std::int64_t amount, bool relative) {
        if (lseek(fd, (off_t) amount, relative ? SEEK_CUR : SEEK_SET) == -1) {
            std::perror("seek_file -> lseek");
            std::abort();
        }
    }

    void prefetch_from_file_at(int fd, std::uint64_t offset, std::size_t length) {
        if (readahead(fd, (off64_t) offset, length) == -1) {
            std::perror("prefetch_from_file -> readahead");
            std::abort();
        }
    }

    std::uint64_t tell_file(int fd) {
        off_t rv = lseek(fd, 0, SEEK_CUR);
        if (rv == -1) {
            std::perror("tell_file -> lseek");
            std::abort();
        }
        return rv;
    }

    void close_file(int fd) {
        if (close(fd) == -1) {
            std::perror("close_file -> close");
            std::abort();
        }
    }
}
