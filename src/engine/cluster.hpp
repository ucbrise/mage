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
  * @file engine/cluster.hpp
  * @brief Tools for intra-party communication among workers running a program
  * using MAGE.
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
    /**
     * @brief Describes an asynchronous read.
     */
    struct AsyncRead {
        void* into;
        std::size_t length;
    };

    /**
     * @brief Represents a communication channel between two workers in the
     * same party, supporting asynchronous receive and buffered send
     * operations.
     *
     * If, in the future, we need to allow multiple implementations (e.g.,
     * sockets, shared memory, etc.), I may change this to an abstract class.
     * For now, we just use sockets.
     */
    class MessageChannel {
    public:
        /**
         * @brief Creates a new MessageChannel that wraps the provided socket
         * file descriptor.
         *
         * @param fd The socket file descriptor to wrap.
         * @param buffer_size The size of the send and receive buffers.
         */
        MessageChannel(int fd, std::size_t buffer_size = 1 << 18);

        /**
         * @brief Creates an invalid MessageChannel that is not suitable for
         * use.
         */
        MessageChannel();

        /**
         * @brief Sends any pending data and closes the underlying file
         * descriptor.
         */
        virtual ~MessageChannel();

        /**
         * @brief Reads data synchronously from the MessageChannel.
         *
         * @tparam T The units in which the amount of data to read is
         * specified.
         * @param buffer A pointer to the memory where the read data should
         * be stored.
         * @param count The amount of data to read from the MessageChannel, in
         * units of @p T.
         */
        template <typename T>
        void read(T* buffer, std::size_t count) {
            AsyncRead& ar = this->start_post_read();
            ar.into = buffer;
            ar.length = sizeof(T) * count;
            this->finish_post_read();
            this->wait_until_reads_finished();
        }

        /**
         * @brief Allocates space to store an asynchronous read, and returns a
         * reference to that memory to be initialized.
         *
         * If memory is not available to store the asynchronous read operation,
         * this operation blocks until enough memory becomes available (which
         * will happen when pending asynchronous reads complete).
         *
         * @return A reference to data storing information about the
         * asynchronous read operation, which the caller must initialize.
         */
        AsyncRead& start_post_read() {
            AsyncRead* ar = this->posted_reads.start_write_in_place(1);
            return *ar;
        }

        /**
         * @brief Indicates to this MessageChannel that the asynchronous read
         * buffer returned by start_post_read() has been initialized, and can
         * now be used to direct received data.
         */
        void finish_post_read() {
            {
                std::lock_guard<std::mutex> lock(this->num_posted_reads_mutex);
                this->num_posted_reads++;
            }
            this->posted_reads.finish_write_in_place(1);
        }

        /**
         * @brief Blocks until there are no more pending asynchronous reads.
         */
        void wait_until_reads_finished() {
            std::unique_lock<std::mutex> lock(this->num_posted_reads_mutex);
            while (this->num_posted_reads != 0) {
                this->no_posted_reads.wait(lock);
            }
        }

        /**
         * @brief Writes data to an in-memory buffer, which will be sent to
         * the recipient at a future time.
         *
         * @tparam T The unit by which the size of the data to send will be
         * measured.
         * @param count The amount of data to send, in units of @p T.
         * @return A pointer to an in-memory buffer to initialize with the
         * data to send.
         */
        template <typename T>
        T* write(std::size_t count) {
            T& buffer = this->writer.write<T>(count * sizeof(T));
            return &buffer;
        }

        /**
         * @brief Initiates the sending of any pending data in this
         * MessageChannel's in-memory buffers.
         */
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
     * @brief Represents this worker's logical "local network" endpoint
     * connecting it to all other workers for this party.
     *
     * It consits of network connections from this worker in the cluster for
     * this party to every other worker in the cluster for this party.
     */
    class ClusterNetwork {
    public:
        /**
         * @brief Creates and (partially) initializes a ClusterNetwork
         * instance.
         *
         * Before contacting any other worker, the establish() function must
         * be called to establish the network. Doing so completes the
         * initialization process.
         *
         * @param self The ID of the worker whose endpoint this ClusterNetwork
         * instance represents.
         * @param buffer_size The size of the send and receive buffers for each
         * other worker in the cluster.
         */
        ClusterNetwork(WorkerID self, std::size_t buffer_size);

        /**
         * @brief Obtains the ID of the worker whose endpoint this
         * ClusterNetwork instance represents.
         *
         * @return The ID of the worker whose endpoint this ClusterNetwork
         * instance represents.
         */
        WorkerID get_self() const;

        /**
         * @brief Returns the total number of workers in this party.
         *
         * The returned result is meaningful only after a successful call to
         * establish().
         *
         * @return The total number of workers in this party.
         */
        WorkerID get_num_workers() const;

        /**
         * @brief Establishes network communication with other workers in this
         * party.
         *
         * @param party The configuration value for this party, providing the
         * internal network host and port numbers of the other workers in the
         * party.
         * @return An empty string if the operation is successful, otherwise
         * a human-readable error message describing the reason for failure.
         */
        std::string establish(const util::ConfigValue& party);

        /**
         * @brief Obtain a pointer to a MessageChannel instance to communicate
         * with the specified worker.
         *
         * @param worker_id The ID of the worker with which to communicate.
         * @return A pointer to a MessageChannel instance to communicate with
         * the specified worker, or a null pointer if the provided @p worker_id
         * is invalid or corresponds to the worker whose endpoint this
         * ClusterNetwork instance represents.
         */
        MessageChannel* contact_worker(WorkerID worker_id) {
            if (worker_id == this->self_id || worker_id >= this->channels.size()) {
                return nullptr;
            }
            return this->channels[worker_id].get();
        }

        /**
         * @brief The maximum number of connection attempts when connecting to
         * other workers.
         */
        static const std::uint32_t max_connection_tries;

        /**
         * @brief The delay between connection attempts when connecting to
         * other workers.
         */
        static const std::chrono::duration<std::uint32_t, std::milli> delay_between_connection_tries;

    private:
        std::vector<std::unique_ptr<MessageChannel>> channels;
        std::size_t channel_buffer_size;
        WorkerID self_id;
    };
}

#endif
