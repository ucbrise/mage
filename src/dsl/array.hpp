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

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <utility>
#include <vector>
#include "addr.hpp"

namespace mage::dsl {
    enum class Layout : std::uint8_t {
        Cyclic,
        Blocked
    };

    template <typename T>
    class DistributedArray {
        template <typename U>
        friend class DistributedArray;

    public:
        DistributedArray(std::size_t length, WorkerID self, WorkerID num_processors, Layout strategy)
            : total_length(length), self_id(self), num_proc(num_processors), layout(strategy) {
            this->num_local_base = this->total_length / this->num_proc;
            this->num_extras = this->total_length % this->num_proc;
            if (this->self_id < this->num_extras) {
                this->local_array.resize(this->num_local_base + 1);
            } else {
                this->local_array.resize(this->num_local_base);
            }
        }

        void for_each(std::function<void(std::size_t, T&)> f) {
            auto [index, stride] = this->get_base_and_stride();
            for (T& elem : this->local_array) {
                f(index, elem);
                index += stride;
            }
        }

        void for_each_pair(std::function<void(std::size_t, T&, T&)> f) {
            if (this->layout != Layout::Blocked) {
                // Not supported yet
                return;
            }
            if (this->self_id != 0) {
                // TODO edge case: what if local_array size is 0?
                this->local_array[0].send(this->self_id - 1);
                T::communication_barrier(this->self_id - 1);
            }
            // TODO
        }

        template <typename U>
        DistributedArray<U> map(std::function<U(std::size_t, T&)> f) {
            DistributedArray<U> rv(this->total_length, this->self_id, this->num_proc, this->layout);
            assert(this->local_array.size() == rv.local_array.size());
            auto [index, stride] = this->get_base_and_stride();
            for (std::size_t i = 0; i != this->local_array.size(); i++) {
                rv.local_array[i] = f(index, this->local_array.size());
                index += stride;
            }
            return rv;
        }

        std::optional<T> reduce(WorkerID gets_result, std::function<T(T&, T&)> f, T&& initial_value) {
            T current = initial_value;
            for (T& elem : this->local_array) {
                current = f(current, elem);
            }
            if (this->self_id == gets_result) {
                std::vector<T> partial_reduction(this->num_proc - 1);
                for (WorkerID i = 0; i != this->num_proc; i++) {
                    if (i < this->self_id) {
                        partial_reduction[i].receive(i);
                    } else if (i > this->self_id) {
                        partial_reduction[i - 1].receive(i);
                    }
                }
                for (T& elem : partial_reduction) {
                    current = f(current, elem);
                }
                return std::move(current);
            } else {
                current.send(gets_result);
                T::communication_barrier(gets_result);
                return std::nullopt;
            }
        }

        std::pair<std::size_t, std::size_t> get_base_and_stride() const {
            std::size_t base;
            std::size_t stride;
            if (this->layout == Layout::Cyclic) {
                base = this->self_id;
                stride = this->num_proc;
            } else if (this->layout == Layout::Blocked) {
                if (this->self_id < this->num_extras) {
                    base = (this->num_local_base + 1) * this->self_id;
                } else {
                    std::size_t boundary = this->num_extras * (this->num_local_base + 1);
                    base = boundary + (this->self_id - this->num_extras) * this->num_local_base;
                }
                stride = 1;
            } else {
                this->layout_panic();
            }
            return std::make_pair(base, stride);
        }

        WorkerID who(std::size_t index) {
            assert(index < this->total_length);
            if (this->layout == Layout::Cyclic) {
                return index % this->num_proc;
            } else if (this->layout == Layout::Blocked) {
                std::size_t boundary = this->num_extras * (this->num_local_base + 1);
                if (index < boundary) {
                    return index / (this->num_local_base + 1);
                } else {
                    return this->num_extras + (index - boundary) / this->num_local_base;
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
