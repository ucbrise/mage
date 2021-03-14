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
 * @file platform/network.hpp
 * @brief System-level utilities for communication using the network.
 */

#ifndef MAGE_PLATFORM_NETWORK_HPP_
#define MAGE_PLATFORM_NETWORK_HPP_

#include <cstddef>
#include <cstdint>

namespace mage::platform {
    /**
     * @brief Describes a network-related error.
     */
    enum class NetworkError : std::uint8_t {
        Success,
        ConnectionRefused,
        TimedOut,
    };

    /**
     * @brief Listens for incoming TCP connections on the specified port and
     * accepts the specified number of connections.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param port The port on which to listen for incoming connections,
     * provided as a string.
     * @param[out] into An array into which to write file descriptors for the
     * accepted connections.
     * @param count The number of incoming connections to accept.
     */
    void network_accept(const char* port, int* into, std::uint32_t count = 1);

    /**
     * @brief Creates the specified number of TCP connections to the endpoint
     * the specified hostname and port.
     *
     * If @p err is not @p nullptr, then it is treated as an array of error
     * conditions where the element at a particular index is populated
     * according to which error, if any occurred when establishing the
     * connection at that index. If it is populated with an error condition
     * (i.e., with something other than @p Success), then that element of
     * the @p into array is left uninitialized. If an error occurs that cannot
     * be described by a @p NetworkError, or if an error occurs and @p err is
     * @p nullptr, then the process is aborted.
     *
     * @param host The hostname of the TCP endpoint to which to connect.
     * @param port The port of the TCP endpoint to which to connect.
     * @param[out] into An array into which to write file descriptors for the
     * resulting connections.
     * @param[out] err An array into which an error condition for each
     * connection is written.
     * @param count The number of connections to establish with the specified
     * TCP endpoint.
     */
    void network_connect(const char* host, const char* port, int* into, NetworkError* err, std::uint32_t count = 1);

    /**
     * @brief Closes a file descriptor corresponding to a TCP connection,
     * shutting down the connection.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param socket The file descriptor to close.
     */
    void network_close(int socket);

    /**
     * @brief Opens a pipe, placing the output file descriptor and input file
     * descriptor, in that order, into the specified array.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param[out] into The array into which to place the pipe's two file
     * descriptors.
     */
    void pipe_open(int* into);

    /**
     * @brief Closes a file descriptor corresponding to a pipe.
     *
     * If an error occurs, then the process is aborted.
     *
     * @param fd The file descriptor to close.
     */
    void pipe_close(int fd);
}

#endif
