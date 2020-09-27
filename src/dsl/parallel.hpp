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

#ifndef MAGE_DSL_PARALLEL_HPP_
#define MAGE_DSL_PARALLEL_HPP_

#include "addr.hpp"

namespace mage::dsl {
    struct ClusterUtils {
        WorkerID self_id;
        WorkerID num_proc;

        template <typename T>
        std::optional<T> reduce_aggregates(WorkerID gets_result, T& local_aggregate, std::function<T(T&, T&)> f) {
            T current = std::move(local_aggregate);
            if (this->self_id == gets_result) {
                std::vector<T> partial_reduction(this->num_proc - 1);
                for (WorkerID i = 0; i != this->num_proc; i++) {
                    if (i < this->self_id) {
                        partial_reduction[i].post_receive(i);
                    } else if (i > this->self_id) {
                        partial_reduction[i - 1].post_receive(i);
                    }
                }
                for (WorkerID i = 0; i != this->num_proc - 1; i++) {
                    T::finish_receive(i < this->self_id ? i : i + 1);
                    current = f(current, partial_reduction[i]);
                }
                return std::move(current);
            } else {
                current.buffer_send(gets_result);
                T::finish_send(gets_result);
                return std::nullopt;
            }
        }
    };
}

#endif
