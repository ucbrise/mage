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
#include <cstdint>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

namespace mage::platform {
    int network_accept(const char* port) {
        struct addrinfo hints = { 0 };
        hints.ai_flags = AI_PASSIVE;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo* info;
        int rv = getaddrinfo(NULL, port, &hints, &info);
        if (rv != 0) {
            std::cerr << "network_accept -> getaddrinfo: " << gai_strerror(rv) << std::endl;
            std::abort();
        }

        int server_socket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        if (server_socket == -1) {
            std::perror("network_accept -> socket");
            std::abort();
        }

        if (bind(server_socket, info->ai_addr, info->ai_addrlen) == -1) {
            std::perror("network_accept -> bind");
            std::abort();
        }

        freeaddrinfo(info);

        if (listen(server_socket, 0) == -1) {
            std::perror("network_accept -> listen");
            std::abort();
        }

        int connection_socket = accept(server_socket, NULL, NULL);
        if (connection_socket == -1) {
            std::perror("network_accept -> accept");
            std::abort();
        }

        if (close(server_socket) == -1) {
            std::perror("network_accept -> close");
            std::abort();
        }

        return connection_socket;
    }

    int network_connect(const char* host, const char* port) {
        struct addrinfo hints = { 0 };
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo* info;
        int rv = getaddrinfo(host, port, &hints, &info);
        if (rv != 0) {
            std::cerr << "network_connect -> getaddrinfo: " << gai_strerror(rv) << std::endl;
            std::abort();
        }

        int connection_socket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        if (connection_socket == -1) {
            std::perror("network_connect -> socket");
            std::abort();
        }

        if (connect(connection_socket, info->ai_addr, info->ai_addrlen) == -1) {
            std::perror("network_connect -> connect");
            std::abort();
        }

        freeaddrinfo(info);

        return connection_socket;
    }

    void network_close(int socket) {
        if (close(socket) == -1) {
            std::perror("network_close -> close");
            std::abort();
        }
    }
}
