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
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "addr.hpp"
#include "util/config.hpp"
#include "util/filebuffer.hpp"
#include "util/userpipe.hpp"

namespace mage::engine {
    struct AsyncRead {
        void* into;
        std::size_t length;
    };

    /*
     * If, in the future, we need to allow multiple implementations (e.g.,
     * sockets, shared memory, etc.), I may change this to an abstract class.
     * For now, we just use sockets.
     */
    class MessageChannel {
    public:
        MessageChannel(int fd, std::size_t buffer_size = 1 << 18);
        MessageChannel();
        virtual ~MessageChannel();

        template <typename T>
        void read(T* buffer, std::size_t count) {
            AsyncRead& ar = this->start_post_read();
            ar.into = buffer;
            ar.length = sizeof(T) * count;
            this->finish_post_read();
            this->wait_until_reads_finished();
        }

        AsyncRead& start_post_read() {
            AsyncRead* ar = this->posted_reads.start_write_in_place(1);
            return *ar;
        }

        void finish_post_read() {
            {
                std::lock_guard<std::mutex> lock(this->num_posted_reads_mutex);
                this->num_posted_reads++;
            }
            this->posted_reads.finish_write_in_place(1);
        }

        void wait_until_reads_finished() {
            std::unique_lock<std::mutex> lock(this->num_posted_reads_mutex);
            while (this->num_posted_reads != 0) {
                this->no_posted_reads.wait(lock);
            }
        }

        template <typename T>
        T* write(std::size_t count) {
            T& buffer = this->writer.write<T>(count * sizeof(T));
            return &buffer;
        }

        void flush() {
            this->writer.flush();
        }

    private:
        void start_reading_daemon();

        util::BufferedFileWriter<false> writer;
        util::BufferedFileReader<false> reader;
        int socket_fd;

        util::UserPipe<AsyncRead> posted_reads;
        std::size_t num_posted_reads;
        std::mutex num_posted_reads_mutex;
        std::condition_variable no_posted_reads;
        std::thread reading_daemon;
    };

    /*
     * Network connections from this worker in the cluster to every other worker
     * in the cluster.
     */
    class ClusterNetwork {
    public:
        ClusterNetwork(WorkerID self, std::size_t buffer_size);

        WorkerID get_self() const;
        WorkerID get_num_workers() const;

        std::string establish(const util::ConfigValue& party);

        MessageChannel* contact_worker(WorkerID worker_id) {
            if (worker_id == this->self_id || worker_id >= this->channels.size()) {
                return nullptr;
            }
            return this->channels[worker_id].get();
        }

        static const std::uint32_t max_connection_tries;
        static const std::chrono::duration<std::uint32_t, std::milli> delay_between_connection_tries;

    private:
        std::vector<std::unique_ptr<MessageChannel>> channels;
        std::size_t channel_buffer_size;
        WorkerID self_id;
    };
}

#endif
