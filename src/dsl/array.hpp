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
 * @file dsl/array.hpp
 * @brief Utility for partitioning arrays over multiple workers when using
 * MAGE's DSLs.
 */

#ifndef MAGE_DSL_ARRAY_HPP_
#define MAGE_DSL_ARRAY_HPP_

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include "addr.hpp"
#include "dsl/util.hpp"
#include "util/misc.hpp"

namespace mage::dsl {
    /**
     * @brief Describes the layout of a ShardedArray, which can be either be
     * a Cyclic layout or a Blocked layout.
     *
     * @sa ShardedArray
     */
    enum class Layout : std::uint8_t {
        Cyclic,
        Blocked
    };

    /**
     * @brief An abstraction for an ordered container partitioned across
     * the workers in a distributed-memory computation.
     *
     * Logically, the data form a single logical array. This array may be
     * partitioned among the workers in two layouts. In the Cyclic layout,
     * elements are strided across the workers. If there are W workers, worker
     * i has elements at indices i, i + W, i + 2W, ... . In the Blocked layout,
     * each worker stores a contiguous section of the logical array. The array
     * is divided into W consecutive sections of equal length (the remainder
     * distributed among the first sections if the division is uneven), and
     * worker i has the elemenths of the ith section.
     *
     * The Sharded Array allows one to switch between the two reprsentations
     * via a communication phase, which is useful for implementing certain
     * algorithms (e.g., bitonic sort, fast Fourier transform) in a distributed
     * memory model.
     *
     * @tparam T The type of elements in the ShardedArray.
     */
    template <typename T>
    class ShardedArray {
        template <typename U>
        friend class ShardedArray;

    public:
        /**
         * @brief Creates a ShardedArray.
         *
         * @param length The total number of elements in the ShardedArray,
         * across all workers.
         * @param self The ID of the worker in whose program this is being
         * called.
         * @param num_processors The total number of workers.
         * @param The initial layout strategy of this ShardedArray.
         */
        ShardedArray(std::size_t length, WorkerID self, WorkerID num_processors, Layout strategy)
            : total_length(length), self_id(self), num_proc(num_processors), layout(strategy) {
            this->num_local_base = this->total_length / this->num_proc;
            this->num_extras = this->total_length % this->num_proc;
            if (this->self_id < this->num_extras) {
                this->local_array.resize(this->num_local_base + 1);
            } else {
                this->local_array.resize(this->num_local_base);
            }
        }

        /**
         * @brief Returns a reference to the underlying data stored locally
         * for this worker.
         *
         * @return A reference to a vector storing the partition of the
         * logical array stored at this worker.
         */
        std::vector<T>& get_locals() {
            return this->local_array;
        }

        /**
         * @brief Returns a const reference to the underlying data stored
         * locally for this worker.
         *
         * @return A const reference to a vector storing the partition of the
         * logical array stored at this worker.
         */
        const std::vector<T>& get_locals() const {
            return this->local_array;
        }

        /**
         * @brief Obtain the layout (partitioning) strategy of this
         * ShardedArray.
         *
         * @return The layout/partitioning strategy of this ShardedArray.
         */
        Layout get_layout() const {
            return this->layout;
        }

        /**
         * @brief Returns the ID of the worker using this ShardedArray
         * instance, provided at the time of construction.
         *
         * @return The ID of the worker whose partition is held by this
         * ShardedArray instance.
         */
        WorkerID get_self_id() const {
            return this->self_id;
        }

        /**
         * @brief Returns the number of workers over which the logical array is
         * distributed, provided at the time of construction.
         *
         * @return The number of workers over which the contents of the Logical
         * array are distributed.
         */
        WorkerID get_num_proc() const {
            return this->num_proc;
        }

        /**
         * @brief Calls the provided function @p f on each element of the array
         * held by this ShardedArray instance.
         *
         * To perform a "for each" operation on the overall array, all workers
         * should call this function. For each element, the provided function
         * will be called on exactly one worker.
         *
         * The element's index in the logical array as the first argument and
         * a reference to the element itself as the second argument.
         *
         * @param f The function to call.
         */
        void for_each(std::function<void(std::size_t, T&)> f) {
            auto [index, stride] = this->get_global_base_and_stride(this->self_id, this->layout);
            for (T& elem : this->local_array) {
                f(index, elem);
                index += stride;
            }
        }

        /**
         * @brief When called concurrently by all workers, calls the provided
         * function @p f on each pair of consecutive elements of the logical
         * array.
         *
         * It is expected that all workers will call this function
         * concurrently, to allow elements to be transferred in cases where
         * consecutive elements of the logical array reside at different
         * workers.
         *
         * The provided function is invoked with three arguments: the index of
         * the first element in the pair in the logical array, a reference to
         * the first element in the pair, and a reference to the second element
         * in the pair. When called concurrently by all workers, the provided
         * function is called exactly once for each pair of consecutive
         * elements in the array, on one of the workers.
         *
         * @param f The function to call.
         */
        void for_each_pair(std::function<void(std::size_t, T&, T&)> f) {
            if (this->layout == Layout::Cyclic) {
                if (this->num_proc == 1) {
                    for (std::size_t i = 0; i + 1 < this->local_array.size(); i++) {
                        f(i, this->local_array[i], this->local_array[i + 1]);
                    }
                    return;
                }

                WorkerID prev = (this->self_id == 0 ? this->num_proc : this->self_id) - 1;
                WorkerID next = (this->self_id + 1) % this->num_proc;

                std::size_t num_pairs = this->local_array.size();
                if (this->self_id == this->num_proc - 1 && num_pairs != 0) {
                    num_pairs--; // very last element is never the first in a pair
                }
                std::vector<T> next_elems(num_pairs);

                std::size_t start = 0;
                if (this->self_id == 0) {
                    start = 1; // very first element of array is never second in a pair
                    if (next_elems.size() != 0) {
                        next_elems[0].post_receive(next);
                    }
                }
                for (std::size_t i = start; i < this->local_array.size(); i++) {
                    this->local_array[i].buffer_send(prev);
                    if (i < next_elems.size()) {
                        next_elems[i].post_receive(next);
                    }
                }
                T::finish_send(prev);
                T::finish_receive(next);

                std::size_t j = 0;
                this->for_each([this, &j, &next_elems, f](std::size_t index, T& elem) {
                    if (index == this->total_length - 1) {
                        // No next element
                        return;
                    }
                    f(index, elem, next_elems[j++]);
                });
            } else if (this->layout == Layout::Blocked) {
                if (this->self_id != 0 && this->get_local_size(this->self_id) != 0) {
                    assert(this->local_array.size() != 0);
                    this->local_array[0].buffer_send(this->self_id - 1);
                    T::finish_send(this->self_id - 1);
                }
                auto [base, stride] = this->get_global_base_and_stride(this->self_id, this->layout);
                for (std::size_t i = 0; i + 1 < this->local_array.size(); i++) {
                    f(base + i, this->local_array[i], this->local_array[i + 1]);
                }
                if (this->self_id != this->num_proc - 1 && this->get_local_size(this->self_id + 1) != 0) {
                    std::size_t num_local = this->local_array.size();
                    T first_next;
                    first_next.post_receive(this->self_id + 1);
                    T::finish_receive(this->self_id + 1);
                    f(base + num_local - 1, this->local_array[num_local - 1], first_next);
                }
            } else {
                this->layout_panic();
            }
        }

        /**
         * @brief When called concurrently by all workers, the contents of the
         * logical array are materialized at each worker.
         *
         * It is expected that all workers will call this function
         * concurrently, so that the partition of the logical array stored at
         * each worker can be sent to and received by all other workers.
         *
         * @param destructive If true, some data may be move-assigned out of
         * the ShardedArray to save memory, in effect making the ShardedArray
         * unusable thereafter. If false, the ShardedArray's contents are
         * preserved.
         */
        std::vector<T> materialize_global_array(bool destructive) {
            if (this->layout != Layout::Blocked) {
                this->layout_panic();
            }

            std::vector<T> globals(this->total_length);
            auto [ base_size, num_extras ] = util::floor_div(this->total_length, this->num_proc);
            for (std::uint64_t i = 0; i != base_size + 1; i++) {
                if (i < this->local_array.size()) {
                    for (WorkerID w = 0; w != this->num_proc; w++) {
                        // std::cout << i << " -> " << w << std::endl;
                        if (w != this->self_id) {
                            this->local_array[i].buffer_send(w);
                        }
                    }
                }
                for (WorkerID w = 0; w != this->num_proc; w++) {
                    std::uint64_t w_base = this->get_global_base_and_stride(w, Layout::Blocked).first;
                    if (i < base_size || w < num_extras) {
                        if (w == this->self_id) {
                            // std::cout << "self: " << i << " -> " << (w_base + i) << std::endl;
                            if (destructive) {
                                globals[w_base + i] = std::move(this->local_array[i]);
                            } else {
                                globals[w_base + i].mutate(this->local_array[i]);
                            }
                        } else {
                            // std::cout << (w_base + i) << " <- " << w << std::endl;
                            globals[w_base + i].post_receive(w);
                        }
                    }
                }
            }

            communication_barrier<T>(this->self_id, this->num_proc);

            return globals;
        }

        /**
         * @brief Initiates a communication phase to change the layout strategy
         * of this ShardedArray.
         *
         * It is expected that all workers call this function concurrently,
         * as all workers need to participate in the communication phase.
         *
         * @param to The layout strategy to use for this ShardedArray.
         */
        void switch_layout(Layout to) {
            if ((this->layout == Layout::Cyclic && to == Layout::Blocked) || (this->layout == Layout::Blocked && to == Layout::Cyclic)) {
                auto [my_current_base, my_current_stride] = this->get_global_base_and_stride(this->self_id, this->layout);
                auto [my_target_base, my_target_stride] = this->get_global_base_and_stride(this->self_id, to);
                std::size_t my_length = this->get_local_size(this->self_id);
                if (my_length == 0) {
                    return;
                }
                std::vector<T> array(my_length);
                for (WorkerID j = 1; j != this->num_proc; j++) {
                    WorkerID i; // send to worker i
                    std::int64_t send_start, send_end, send_stride;
                    WorkerID k; // receive from worker k
                    std::int64_t recv_start, recv_end, recv_stride;
                    {
                        i = (this->self_id + j) % this->num_proc;
                        /* Send to Worker i everything that is relevant to that worker. */
                        auto [i_target_base, i_target_stride] = this->get_global_base_and_stride(i, to);
                        std::size_t i_length = this->get_local_size(i);

                        if (this->layout == Layout::Cyclic && to == Layout::Blocked) {
                            send_start = util::ceil_div(i_target_base - my_current_base, my_current_stride).first;
                            send_end = util::floor_div((i_target_base + (i_length - 1)) - my_current_base, my_current_stride).first;
                        } else {
                            send_start = util::floor_div(i_target_base - my_current_base, my_target_stride).second;
                            send_end = my_length - 1;
                        }
                        send_stride = i_target_stride;
                    }
                    {
                        k = (this->self_id - j + this->num_proc) % this->num_proc;
                        /* Receive from Worker k. */
                        auto [k_current_base, k_current_stride] = this->get_global_base_and_stride(k, this->layout);
                        std::size_t k_length = this->get_local_size(k);

                        if (this->layout == Layout::Cyclic && to == Layout::Blocked) {
                            recv_start = util::floor_div(k_current_base - my_target_base, k_current_stride).second;
                            recv_end = my_length - 1;
                        } else {
                            recv_start = util::ceil_div(k_current_base - my_target_base, my_target_stride).first;
                            recv_end = util::floor_div((k_current_base + (k_length - 1) * k_current_stride) - my_target_base, my_target_stride).first;
                        }
                        recv_stride = k_current_stride;
                    }

                    std::int64_t s = send_start;
                    std::int64_t r = recv_start;
                    while (s <= send_end || r <= recv_end) {
                        if (s <= send_end) {
                            // std::cout << "Sending " << s << std::endl;
                            this->local_array[s].buffer_send(i);
                            s += send_stride;
                        }
                        if (r <= recv_end) {
                            // std::cout << "Receiving " << r << std::endl;
                            array[r].post_receive(k);
                            r += recv_stride;
                        }
                    }
                    T::finish_send(i);
                    T::finish_receive(k);
                }
                {
                    /* Shuffle the elements that need to be kept locally. */
                    std::size_t from_start;
                    std::size_t to_start;
                    if (this->layout == Layout::Cyclic && to == Layout::Blocked) {
                        from_start = util::ceil_div(my_target_base - my_current_base, my_current_stride).first;
                        to_start = util::floor_div(my_current_base - my_target_base, my_current_stride).second;
                    } else {
                        from_start = util::floor_div(my_target_base - my_current_base, my_target_stride).second;
                        to_start = util::ceil_div(my_current_base - my_target_base, my_target_stride).first;
                    }
                    for (std::size_t to = to_start, from = from_start; to < my_length && from < my_length; (to += my_current_stride), (from += my_target_stride)) {
                        // std::cout << "Shuffling " << from << " -> " << to << std::endl;
                        array[to] = std::move(this->local_array[from]);
                    }
                }
                this->local_array = std::move(array);
                this->layout = to;

                for (WorkerID k = 0; k != this->num_proc; k++) {
                    if (k != this->self_id) {
                        T::finish_receive(k);
                    }
                }
            } else if (this->layout != to) {
                this->operation_panic("cannot switch layout");
            }
        }

        /**
         * @brief For a given worker and layout strategy, obtains the global
         * index of the first element in that worker's local array and the
         * difference in global index between consecutive elements of that
         * worker's local array.
         *
         * Here, "global index" refers to the index of the element in the
         * logical array partitioned over the workers.
         *
         * @param id The ID of the worker in question.
         * @param layout The layout strategy to consider.
         * @return A pair where the first element is the global index of the
         * first element in that worker's local array and the second element
         * is the difference in global index between consecutive elements of
         * that worker's local array.
         */
        std::pair<std::int64_t, std::int64_t> get_global_base_and_stride(WorkerID id, Layout layout) const {
            std::int64_t base;
            std::int64_t stride;
            if (layout == Layout::Cyclic) {
                base = id;
                stride = this->num_proc;
            } else if (layout == Layout::Blocked) {
                if (id < this->num_extras) {
                    base = (this->num_local_base + 1) * id;
                } else {
                    std::int64_t boundary = this->num_extras * (this->num_local_base + 1);
                    base = boundary + (id - this->num_extras) * this->num_local_base;
                }
                stride = 1;
            } else {
                this->layout_panic();
            }
            return std::make_pair(base, stride);
        }

        /**
         * @brief Obtains the global index of the first element in this
         * ShardedArray instance's local data and the difference in global
         * index between consecutive elements of this ShardedArray instance's
         * local data.
         *
         * Here, "global index" refers to the index of the element in the
         * logical array partitioned over the workers.
         *
         * @return A pair where the first element is the global index of the
         * first element in this ShardedArray instance's local data and the
         * second element is the difference in global index between consecutive
         * elements of this ShardedArray instance's local data.
         */
        std::pair<std::int64_t, std::int64_t> get_global_base_and_stride() const {
            return this->get_global_base_and_stride(this->self_id, this->layout);
        }

        /**
         * @brief Obtains the number of elements of the logical array stored by
         * the specified worker, with the current layout strategy.
         *
         * @param who The ID of the worker in question.
         * @return The number of elements stored at the specfied worker.
         */
        std::size_t get_local_size(WorkerID who) const {
            if (who < num_extras) {
                return this->num_local_base + 1;
            } else {
                return this->num_local_base;
            }
        }

        /**
         * @brief Obtains the total number of elements in the logical array.
         *
         * @return The total number of elements in the logical array.
         */
        std::size_t get_total_size() const {
            return this->total_length;
        }

        /**
         * @brief Obtains the worker who should store the element at the
         * provided index in the logical array, with the current layout
         * strategy.
         *
         * @param global_index The index of the element in question in the
         * logical array.
         * @return The ID of the worker that should store the specified element
         * under the current layout strategy.
         */
        WorkerID who(std::size_t global_index) const {
            assert(global_index < this->total_length);
            if (this->layout == Layout::Cyclic) {
                return global_index % this->num_proc;
            } else if (this->layout == Layout::Blocked) {
                std::size_t boundary = this->num_extras * (this->num_local_base + 1);
                if (global_index < boundary) {
                    return global_index / (this->num_local_base + 1);
                } else {
                    return this->num_extras + (global_index - boundary) / this->num_local_base;
                }
            } else {
                this->layout_panic();
            }
        }

    private:
        [[noreturn]] void layout_panic() const {
            std::cerr << "Unknown layout " << static_cast<std::uint8_t>(this->layout) << std::endl;
            std::abort();
        }

        [[noreturn]] static void operation_panic(std::string details) {
            std::cerr << "Unsupported operation: " << details << std::endl;
            std::abort();
        }

        std::vector<T> local_array;
        std::size_t total_length;
        std::size_t num_local_base;
        std::size_t num_extras;
        WorkerID self_id;
        WorkerID num_proc;
        Layout layout;
    };
}

#endif
