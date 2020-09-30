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
#include "util/misc.hpp"

namespace mage::dsl {
    enum class Layout : std::uint8_t {
        Cyclic,
        Blocked
    };

    template <typename T>
    class ShardedArray {
        template <typename U>
        friend class ShardedArray;

    public:
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

        std::vector<T>& get_locals() {
            return this->local_array;
        }

        const std::vector<T>& get_locals() const {
            return this->local_array;
        }

        Layout get_layout() const {
            return this->layout;
        }

        WorkerID get_num_proc() const {
            return this->num_proc;
        }

        void for_each(std::function<void(std::size_t, T&)> f) {
            auto [index, stride] = this->get_global_base_and_stride(this->self_id, this->layout);
            for (T& elem : this->local_array) {
                f(index, elem);
                index += stride;
            }
        }

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

        void switch_layout(Layout to) {
            if (this->layout == Layout::Cyclic && to == Layout::Blocked) {
                auto [my_cyclic_base, my_cyclic_stride] = this->get_global_base_and_stride(this->self_id, Layout::Cyclic);
                auto [my_blocked_base, my_blocked_stride] = this->get_global_base_and_stride(this->self_id, Layout::Blocked);
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
                        auto [i_blocked_base, i_blocked_stride] = this->get_global_base_and_stride(i, Layout::Blocked);
                        std::size_t i_length = this->get_local_size(i);

                        /*
                         * base + local_index * stride = global_index
                         * So local_index = (global_index - base / stride);
                         */
                        std::int64_t local_start = util::ceil_div(i_blocked_base - my_cyclic_base, my_cyclic_stride).first;
                        std::int64_t local_end = util::floor_div((i_blocked_base + i_length - 1) - my_cyclic_base, my_cyclic_stride).first;

                        send_start = local_start;
                        send_end = local_end;
                        send_stride = 1;
                    }
                    {
                        k = (this->self_id - j + this->num_proc) % this->num_proc;
                        /* Receive from Worker k. */
                        auto [k_cyclic_base, k_cyclic_stride] = this->get_global_base_and_stride(k, Layout::Cyclic);
                        std::size_t start_offset = util::floor_div(k_cyclic_base - my_blocked_base, k_cyclic_stride).second;

                        recv_start = start_offset;
                        recv_end = my_length - 1;
                        recv_stride = k_cyclic_stride;
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
                    std::size_t from_start = util::ceil_div(my_blocked_base - my_cyclic_base, my_cyclic_stride).first;
                    // std::size_t from_end = util::floor_div((my_blocked_base + my_length - 1) - my_cyclic_base, my_cyclic_stride).first;
                    std::size_t to_start = util::floor_div(my_cyclic_base - my_blocked_base, my_cyclic_stride).second;
                    for (std::size_t to = to_start, from = from_start; to < my_length; (to += my_cyclic_stride), from++) {
                        // std::cout << "Shuffling " << from << " -> " << to << std::endl;
                        array[to] = std::move(this->local_array[from]);
                    }
                }
                this->local_array = std::move(array);
                this->layout = Layout::Blocked;

                for (WorkerID k = 0; k != this->num_proc; k++) {
                    if (k != this->self_id) {
                        T::finish_receive(k);
                    }
                }
            } else {
                this->operation_panic("cannot switch layout");
            }
        }

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

        std::size_t get_local_size(WorkerID who) const {
            if (who < num_extras) {
                return this->num_local_base + 1;
            } else {
                return this->num_local_base;
            }
        }

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
        void layout_panic() const {
            std::cerr << "Unknown layout " << static_cast<std::uint8_t>(this->layout) << std::endl;
            std::abort();
        }

        static void operation_panic(std::string details) {
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
