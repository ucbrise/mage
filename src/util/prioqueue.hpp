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

#ifndef MAGE_UTIL_PRIOQUEUE_HPP_
#define MAGE_UTIL_PRIOQUEUE_HPP_

#include <cassert>
#include <cstdint>
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace mage::util {
    /*
     * Unlike std::priority_queue, this priority queue supports the decreasekey
     * operation.
     */
    template <typename K, typename V>
    class PriorityQueue {
    public:
        PriorityQueue() {
        }

        bool empty() const {
            return this->data.empty();
        }

        std::uint64_t size() const {
            return this->data.size();
        }

        std::pair<K, V>& min() {
            assert(!this->empty());
            return this->data[0];
        }

        std::pair<K, V> remove_min() {
            assert(!this->empty());

            std::pair<K, V> top = this->data[0];
            this->locator.erase(top.second);

            Index newsize = this->data.size() - 1;
            if (newsize != 0) {
                const std::pair<K, V>& last = this->data[newsize];
                Index i = this->bubbleDown(0, last.first, newsize);
                this->update(i, last);
            }
            this->data.resize(newsize);

            return top;
        }

        std::pair<K, V> remove_second_min() {
            Index newsize = this->data.size() - 1;
            assert(newsize != 0);

            if (newsize == 1) {
                std::pair<K, V> second = this->data[1];
                this->data.pop_back();
                this->locator.erase(second.second);
                return second;
            }

            Index start = this->data[1].first < this->data[2].first ? 1 : 2;
            std::pair<K, V> second = this->data[start];
            this->locator.erase(second.second);
            if (start != newsize) {
                const std::pair<K, V>& last = this->data[newsize];
                Index i = this->bubbleDown(start, last.first, newsize);
                this->update(i, last);
            }
            this->data.resize(newsize);
            return second;
        }

        void insert(const K& key, const V& value) {
            Index prevsize = this->data.size();
            this->data.resize(prevsize + 1);
            Index i = this->bubbleUp(prevsize, key);
            this->set(i, std::make_pair(key, value));
        }

        void erase(const V& value) {
            auto iter = this->locator.find(value);
            assert(iter != this->locator.end());
            Index j = iter->second;
            this->locator.erase(iter);

            Index newsize = this->data.size() - 1;
            if (j != newsize) {
                const std::pair<K, V>& last = this->data[newsize];
                Index i = this->bubbleUp(j, last.first);
                if (i == j) {
                    i = this->bubbleDown(j, last.first, newsize);
                }
                this->update(i, last);
            }
            this->data.resize(newsize);
        }

        const K& get_key(const V& value) {
            Index i = this->locator.at(value);
            return this->data[i].first;
        }

        void decrease_key(const K& newkey, const V& value) {
            Index i = this->locator.at(value);
            if (newkey == this->data[i].first) {
                return;
            }
            assert(newkey < this->data[i].first);
            i = this->bubbleUp(i, newkey);
            this->update(i, std::make_pair(newkey, value));
        }

        void increase_key(const K& newkey, const V& value) {
            Index i = this->locator.at(value);
            if (newkey == this->data[i].first) {
                return;
            }
            assert(newkey > this->data[i].first);
            i = this->bubbleDown(i, newkey, this->data.size());
            this->update(i, std::make_pair(newkey, value));
        }

        bool contains(const V& value) {
            return this->locator.find(value) != this->locator.end();
        }


    private:
        using Index = std::uint64_t;

        static Index parent(Index child) {
            assert(child != 0);
            return ((child + 1) >> 1) - 1;
        }

        static Index leftChild(Index parent) {
            return ((parent + 1) << 1) - 1;
        }

        static Index rightChild(Index parent) {
            return (parent + 1) << 1;
        }

        Index bubbleUp(Index i, const K& key) {
            Index p;
            while (i != 0 && key < this->data[p = parent(i)].first) {
                this->update(i, this->data[p]);
                i = p;
            }
            return i;
        }

        Index bubbleDown(Index i, const K& key, Index size) {
            Index left, right;
            while ((left = (right = rightChild(i)) - 1) < size) {
                Index chosen;
                if (right == size || this->data[left].first < this->data[right].first) {
                    chosen = left;
                } else {
                    chosen = right;
                }
                if (this->data[chosen].first < key) {
                    this->update(i, this->data[chosen]);
                    i = chosen;
                } else {
                    break;
                }
            }
            return i;
        }

        void update(Index i, const std::pair<K, V>& item) {
            this->data[i] = item;
            this->locator.at(item.second) = i;
        }

        void set(Index i, const std::pair<K, V>& item) {
            this->data[i] = item;
            assert(this->locator.find(item.second) == this->locator.end());
            this->locator[item.second] = i;
        }

        std::vector<std::pair<K, V>> data;
        std::unordered_map<V, Index> locator;
    };
}

#endif
