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

#ifndef MAGE_ENGINE_CLUSTER_HPP_
#define MAGE_ENGINE_CLUSTER_HPP_

#include <cstdint>
#include <chrono>
#include "addr.hpp"
#include "util/filebuffer.hpp"
#include "util/resourceset.hpp"

namespace mage::engine {
    /*
     * If, in the future, we need to allow multiple implementations (e.g.,
     * sockets, shared memory, etc.), I may change this to an abstract class.
     * For now, we just use sockets.
     */
    class MessageChannel {
    public:
        MessageChannel(int fd);
        MessageChannel();
        MessageChannel(MessageChannel&& other);
        virtual ~MessageChannel();

        template <typename T>
        T* read(std::size_t count) {
            T& buffer = this->reader.read<T>(count * sizeof(T));
            return &buffer;
        }

        template <typename T>
        T* write(std::size_t count) {
            T& buffer = this->writer.write<T>(count * sizeof(T));
            return &buffer;
        }

        void flush() {
            this->writer.flush();
        }

        void rebuffer() {
            this->reader.rebuffer();
        }

    private:
        util::BufferedFileWriter<false> writer;
        util::BufferedFileReader<false> reader;
        int socket_fd;
    };

    /*
     * Network connections from this worker in the cluster to every other worker
     * in the cluster.
     */
    class ClusterNetwork {
    public:
        ClusterNetwork(WorkerID self);

        WorkerID get_self() const;

        std::string establish(const util::ResourceSet::Party& party);

        MessageChannel* contact_worker(WorkerID worker_id) {
            if (worker_id == this->self_id || worker_id >= this->channels.size()) {
                return nullptr;
            }
            return &this->channels[worker_id];
        }

        static const std::uint32_t max_connection_tries;
        static const std::chrono::duration<std::uint32_t, std::milli> delay_between_connection_tries;

    private:
        std::vector<MessageChannel> channels;
        WorkerID self_id;
    };
}

#endif
