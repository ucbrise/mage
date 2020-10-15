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

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "engine/cluster.hpp"
#include "platform/filesystem.hpp"
#include "platform/network.hpp"
#include "util/config.hpp"
#include "util/filebuffer.hpp"
#include "util/userpipe.hpp"

namespace mage::engine {
    MessageChannel::MessageChannel(int fd) : reader(fd), writer(fd), socket_fd(fd),
        posted_reads(14), num_posted_reads(0) {
        if (fd != -1) {
            this->start_reading_daemon();
        }
    }

    MessageChannel::MessageChannel() : MessageChannel(-1) {
    }

    MessageChannel::~MessageChannel() {
        if (this->socket_fd != -1) {
            this->writer.flush();
            this->posted_reads.close();
            if (this->reading_daemon.joinable()) {
                this->reading_daemon.join();
            }
            platform::network_close(this->socket_fd);
        }
    }

    void MessageChannel::start_reading_daemon() {
        assert(!this->reading_daemon.joinable());
        this->reading_daemon = std::thread([this]() {
            AsyncRead* read_op;
            while ((read_op = this->posted_reads.start_read_single_in_place()) != nullptr) {
                std::uint8_t* buffer = &(this->reader.start_read<std::uint8_t>(read_op->length));
                std::copy(buffer, buffer + read_op->length, static_cast<std::uint8_t*>(read_op->into));
                this->reader.finish_read(read_op->length);
                this->posted_reads.finish_read_single_in_place();

                {
                    std::lock_guard<std::mutex> lock(this->num_posted_reads_mutex);
                    this->num_posted_reads--;
                    if (this->num_posted_reads == 0) {
                        this->no_posted_reads.notify_all();
                    }
                }
            }
        });
    }

    const std::uint32_t ClusterNetwork::max_connection_tries = 20;
    const std::chrono::duration<std::uint32_t, std::milli> ClusterNetwork::delay_between_connection_tries(3000);

    ClusterNetwork::ClusterNetwork(WorkerID self) : channels(), self_id(self) {
    }

    WorkerID ClusterNetwork::get_self() const {
        return this->self_id;
    }

    WorkerID ClusterNetwork::get_num_workers() const {
        return this->channels.size();
    }

    std::string ClusterNetwork::establish(const util::ConfigValue& party) {
        WorkerID num_workers = party["workers"].get_size();
        if (this->self_id >= num_workers) {
            return "Self index is " + std::to_string(this->self_id) + " but num_workers is " + std::to_string(num_workers);
        }

        for (WorkerID i = 0; i != num_workers; i++) {
            const util::ConfigValue& worker = party["workers"][i];
            if (worker.get("internal_host") == nullptr || worker.get("internal_port") == nullptr) {
                return "Insufficient internal network information for worker " + std::to_string(i);
            }
        }

        std::vector<int> fds;
        fds.resize(num_workers);
        std::fill(fds.begin(), fds.end(), -1);

        bool success[num_workers];
        std::fill(&success[0], &success[num_workers], false);
        success[this->self_id] = true;

        /* Connect to all workers with smaller index. */
        /* TODO: use an event loop or bounded thread pool to do this. */
        std::vector<std::thread> connectors;
        connectors.resize(self_id);
        for (WorkerID j = 0; j != self_id; j++) {
            connectors[j] = std::thread([&](WorkerID i) {
                const util::ConfigValue& worker = party["workers"][i];
                for (std::uint32_t k = 0; k != ClusterNetwork::max_connection_tries; k++) {
                    platform::NetworkError err;
                    platform::network_connect(worker["internal_host"].as_string().c_str(), worker["internal_port"].as_string().c_str(), &fds[i], &err);
                    if (err == platform::NetworkError::Success) {
                        platform::write_to_file(fds[i], &self_id, sizeof(self_id));
                        success[i] = true;
                        return;
                    } else if (err == platform::NetworkError::ConnectionRefused) {
                        std::this_thread::sleep_for(ClusterNetwork::delay_between_connection_tries);
                    } else if (err == platform::NetworkError::TimedOut) {
                        break;
                    } else {
                        std::cerr << "Unrecognized network error " << static_cast<std::uint32_t>(err) << std::endl;
                        std::abort();
                    }
                }
                success[i] = false;
            }, j);
        }

        /* Accept connections from all workers with a larger index. */
        WorkerID remaining = num_workers - self_id - 1;
        std::vector<int> accept_fds;
        accept_fds.resize(remaining);

        /*
         * TODO: make sure to only accept connections from the internal_host
         * specified in the configuration file.
         */
        if (remaining != 0) {
            platform::network_accept(party["workers"][this->self_id]["internal_port"].as_string().c_str(), accept_fds.data(), remaining);
            for (WorkerID i = 0; i != remaining; i++) {
                WorkerID from;
                platform::read_from_file(accept_fds[i], &from, sizeof(from));
                if (from > self_id && from < num_workers && fds[from] == -1) {
                    success[from] = true;
                    fds[from] = accept_fds[i];
                } else {
                    platform::network_close(accept_fds[i]);
                }
            }
        }

        for (WorkerID i = 0; i != self_id; i++) {
            connectors[i].join();
        }

        std::string rv;
        bool overall_success = true;
        for (WorkerID i = 0; i != num_workers; i++) {
            if (!success[i]) {
                if (overall_success) {
                    rv = "Could not connect to worker(s) " + std::to_string(i);
                } else {
                    rv += (", " + std::to_string(i));
                }
                overall_success = false;
            }
        }
        if (overall_success) {
            this->channels.resize(num_workers);
            for (WorkerID i = 0; i != num_workers; i++) {
                this->channels[i] = std::make_unique<MessageChannel>(fds[i]);
            }
        } else {
            for (WorkerID i = 0; i != num_workers; i++) {
                if (fds[i] != -1) {
                    platform::network_close(fds[i]);
                }
            }
        }

        return rv;
    }
}
