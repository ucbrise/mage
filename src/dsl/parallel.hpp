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
#include "dsl/array.hpp"
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace mage::dsl {
    struct ClusterUtils {
        WorkerID self_id;
        WorkerID num_proc;

        template <typename T>
        std::optional<T> reduce_aggregates(WorkerID gets_result, T& local_aggregate, std::function<T(T&, T&)> f) const {
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

        /*
         * Reorganizes elements of A and B, assigning each element of each list
         * to multiple workers, so that, for each (a, b) in A x B, exactly one
         * has both a and b.
         */
        template <typename T>
        std::pair<std::vector<T>, std::vector<T>> cross_product(ShardedArray<T>& a, ShardedArray<T>& b) const {
            assert(a.get_layout() == Layout::Blocked);
            assert(b.get_layout() == Layout::Blocked);

            /*
             * Initially, each array is divided into partitions. Initially,
             * each worker has one partition.
             */
            std::uint32_t num_partitions = this->num_proc;
            std::uint32_t partition_size_a = a.get_total_size() / num_partitions;
            assert(a.get_total_size() % num_partitions == 0); // for now. This shouldn't be too hard to fix.
            std::uint32_t partition_size_b = b.get_total_size() / num_partitions;
            assert(b.get_total_size() % num_partitions == 0); // for now. This shouldn't be too hard to fix.

            /*
             * After the shuffle, each worker has one portion of each array,
             * where each portion consists of multiple partitions. Each portion
             * is assigned to multiple workers.
             */

            assert(util::is_power_of_two(this->num_proc));
            std::uint8_t log_num_workers = util::log_base_2(this->num_proc);
            std::uint32_t num_portions_a = UINT32_C(1) << ((log_num_workers / 2) + (log_num_workers % 2));
            std::uint32_t num_portions_b = UINT32_C(1) << (log_num_workers / 2);
            std::uint32_t partitions_per_portion_a = num_partitions / num_portions_a;
            std::uint32_t partitions_per_portion_b = num_partitions / num_portions_b;
            std::uint32_t portion_size_a = a.get_total_size() / num_portions_a;
            std::uint32_t portion_size_b = a.get_total_size() / num_portions_b;

            /*
             * These variables store the portion of each array assigned to this
             * worker.
             */
            std::vector<T> my_a(portion_size_a);
            std::vector<T> my_b(portion_size_b);

            /* Figure out who has the partitions I need. */
            std::uint32_t my_target_portion_a = this->self_id / num_portions_b;
            std::uint32_t my_target_portion_b = this->self_id % num_portions_b;
            WorkerID first_needed_partition_owner_a = my_target_portion_a * partitions_per_portion_a;
            WorkerID first_needed_partition_owner_b = my_target_portion_b * partitions_per_portion_b;

            /* Figure out who needs the blocks that I have. */
            std::uint32_t my_current_portion_a = this->self_id / partitions_per_portion_a;
            std::uint32_t my_current_portion_b = this->self_id / partitions_per_portion_b;
            WorkerID first_needing_my_partition_a = my_current_portion_a * num_portions_b; // contiguous
            WorkerID first_needing_my_partition_b = my_current_portion_b; // strided by num_portions_b

            /* Shuffle array A. */
            std::vector<T>& a_locals = a.get_locals();
            for (std::uint32_t i = 0; i != partition_size_a; i++) {
                for (WorkerID to_delta = 0; to_delta != num_portions_b; to_delta++) {
                    WorkerID to = first_needing_my_partition_a + to_delta;
                    // std::cout << "A: " << i << " -> " << to << std::endl;
                    if (to != this->self_id) {
                        a_locals[i].buffer_send(to);
                    }
                    /*
                     * If to == this->self_id, we handle it in the next loop.
                     * We don't std::move it now, since we still need
                     * b_locals[i] to send to the other workers.
                     */
                }
                for (WorkerID from_delta = 0; from_delta != partitions_per_portion_a; from_delta++) {
                    WorkerID from = first_needed_partition_owner_a + from_delta;
                    std::uint32_t into = from_delta * partition_size_a + i;
                    // std::cout << "A: " << into << " <- " << from << std::endl;
                    if (from == this->self_id) {
                        my_a[into] = std::move(a_locals[i]);
                    } else {
                        my_a[into].post_receive(from);
                    }
                }
            }

            /* Shuffle array B. */
            std::vector<T>& b_locals = b.get_locals();
            for (std::uint32_t i = 0; i != partition_size_b; i++) {
                for (WorkerID to = first_needing_my_partition_b; to < this->num_proc; to += num_portions_b) {
                    // std::cout << "B: " << i << " -> " << to << std::endl;
                    if (to != this->self_id) {
                        b_locals[i].buffer_send(to);
                    }
                    /*
                     * If to == this->self_id, we handle it in the next loop.
                     * We don't std::move it now, since we still need
                     * b_locals[i] to send to the other workers.
                     */
                }
                for (WorkerID from_delta = 0; from_delta != partitions_per_portion_b; from_delta++) {
                    WorkerID from = first_needed_partition_owner_b + from_delta;
                    std::uint32_t into = from_delta * partition_size_b + i;
                    // std::cout << "B: " << into << " <- " << from << std::endl;
                    if (from == this->self_id) {
                        my_b[into] = std::move(b_locals[i]);
                    } else {
                        my_b[into].post_receive(from);
                    }
                }
            }

            /* Barriers. */
            for (WorkerID w = 0; w != this->num_proc; w++) {
                if (w != this->self_id) {
                    T::finish_send(w);
                }
            }
            for (WorkerID w = 0; w != this->num_proc; w++) {
                if (w != this->self_id) {
                    T::finish_receive(w);
                }
            }

            return std::make_pair(std::move(my_a), std::move(my_b));
        }
    };
}

#endif
