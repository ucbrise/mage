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
 * @file util/prioqueue.hpp
 * @brief Priority queue data structure.
 */

#ifndef MAGE_UTIL_PRIOQUEUE_HPP_
#define MAGE_UTIL_PRIOQUEUE_HPP_

#include <cassert>
#include <cstdint>
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace mage::util {
    /**
     * @brief A priority queue data structure that supports adjusting keys of
     * elements (key-value pairs) in the priority queue.
     *
     * Although a priority queue is provided in the C++ standard library via
     * the std::priority_queue class, that data structure does not allow one
     * to adjust the value of a key that is already in the priority queue. This
     * class implements that functionality.
     *
     * Multiple elements in the priority queue may have the same key, but no
     * two elements in the priority queue should have the same value.
     *
     * @tparam K The type of keys in the priority queue. Keys must be
     * copy-constructible, copy-assignable, and comparable via the \< operator.
     * @tparam V The type of values in the priority queue. Values must be
     * copy-constructible and copy-assignable.
     */
    template <typename K, typename V>
    class PriorityQueue {
    public:
        /**
         * @brief Creates an empty priority queue.
         */
        PriorityQueue() {
        }

        /**
         * @brief Checks if this priority queue is empty.
         *
         * @return True if this priority queue is empty, false otherwise.
         */
        bool empty() const {
            return this->data.empty();
        }

        /**
         * @brief Returns the number of elements (key-value pairs) in this
         * priority queue.
         *
         * @return The number of elements (key-value pairs) in this priority
         * queue.
         */
        std::uint64_t size() const {
            return this->data.size();
        }

        /**
         * @brief Returns a reference to an element (key-value pair) with the
         * smallest key.
         *
         * If the smallest key is not unique to a single element, then it is
         * undefined which of those elements the returned reference refers to.
         *
         * @return A reference to an element (key-value pair) with the
         * smallest key.
         */
        std::pair<K, V>& min() {
            assert(!this->empty());
            return this->data[0];
        }

        /**
         * @brief Removes an element (key-value pair) with the smallest key
         * from the priority queue and returns it.
         *
         * If the smallest key is not unique to a single element, then it is
         * undefined which of those elements is removed and returned.
         *
         * @return An element (key-value pair) with the smallest key.
         */
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

        /**
         * @brief Removes an element (key-value pair) with the second smallest
         * key from the priority queue and returns it.
         *
         * If the second smallest key is not unique to a single element, then
         * it is undefined which of those elements is removed and returned.
         *
         * @return An element (key-value pair) with the second smallest key.
         */
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

        /**
         * @brief Inserts an element (key-value pair) into the priority queue.
         *
         * @param key The new element's key.
         * @param value The new element's value.
         */
        void insert(const K& key, const V& value) {
            Index prevsize = this->data.size();
            this->data.resize(prevsize + 1);
            Index i = this->bubbleUp(prevsize, key);
            this->set(i, std::make_pair(key, value));
        }

        /**
         * @brief Removes an element with the specified value from the priority
         * queue.
         *
         * @param value The value of the element to be removed.
         */
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

        /**
         * @brief Get the key corresponding to the specified value.
         *
         * @param value The value whose key to look up.
         * @return A reference to the key corresponding to the specified value.
         */
        const K& get_key(const V& value) {
            Index i = this->locator.at(value);
            return this->data[i].first;
        }

        /**
         * @brief Reduce the key corresponding to the specified value.
         *
         * @pre An element with the specified value is in the priority queue,
         * and the new key is less than or equal to the existing key for that
         * value.
         * @post The key corresponding to the specified value is changed to
         * the newly specified key, and the priority queue's internal state is
         * updated accordingly.
         *
         * @param newkey The new (reduced) key.
         * @param value The value whose key to update.
         */
        void decrease_key(const K& newkey, const V& value) {
            Index i = this->locator.at(value);
            if (newkey == this->data[i].first) {
                return;
            }
            assert(newkey < this->data[i].first);
            i = this->bubbleUp(i, newkey);
            this->update(i, std::make_pair(newkey, value));
        }

        /**
         * @brief Increase the key corresponding to the specified value.
         *
         * @pre An element with the specified value is in the priority queue,
         * and the new key is greater than or equal to the existing key for
         * that value.
         * @post The key corresponding to the specified value is changed to
         * the newly specified key, and the priority queue's internal state is
         * updated accordingly.
         *
         * @param newkey The new (reduced) key.
         * @param value The value whose key to update.
         */
        void increase_key(const K& newkey, const V& value) {
            Index i = this->locator.at(value);
            if (newkey == this->data[i].first) {
                return;
            }
            assert(newkey > this->data[i].first);
            i = this->bubbleDown(i, newkey, this->data.size());
            this->update(i, std::make_pair(newkey, value));
        }

        /**
         * @brief Checks if an element (key-value pair) with the specified
         * value is present in the priority queue.
         *
         * @param value The value whose presence to check for.
         * @return True if an element with the specified value is present,
         * otherwise false.
         */
        bool contains(const V& value) {
            return this->locator.find(value) != this->locator.end();
        }


    private:
        /**
         * @brief Type representing an index in the underlying array
         * representation.
         */
        using Index = std::uint64_t;

        /**
         * @brief Given the index of an element in the priority queue's
         * underlying array representation, find the index of the element's
         * parent.
         *
         * @pre The specified element is part of the priority queue and is not
         * the root of the priority queue.
         *
         * @param child The index of an element in the priority queue.
         * @return The index of the specified element's parent.
         */
        static Index parent(Index child) {
            assert(child != 0);
            return ((child + 1) >> 1) - 1;
        }

        /**
         * @brief Given the index of an element in the priority queue's
         * underlying array representation, find the index of the element's
         * left child.
         *
         * @pre The specified element is part of the priority queue and has a
         * left child in the priority queue.
         *
         * @param parent The index of an element in the priority queue.
         * @return The index of the element's left child.
         */
        static Index leftChild(Index parent) {
            return ((parent + 1) << 1) - 1;
        }

        /**
         * @brief Given the index of an element in the priority queue's
         * underlying array representation, find the index of the element's
         * right child.
         *
         * @pre The specified element is part of the priority queue and has a
         * right child in the priority queue.
         *
         * @param parent The index of an element in the priority queue.
         * @return The index of the element's right child.
         */
        static Index rightChild(Index parent) {
            return (parent + 1) << 1;
        }

        /**
         * @brief Considering only the array prefix of the priority queue up
         * to the specified index, find the index at which an element with the
         * specified key should be inserted, rearranging the existing elements
         * to make space for the element at that index.
         *
         * @param i The last element of the array prefix in consideration,
         * considered an empty slot when rearranging the priority queue.
         * @param key The key for the element whose insertion is considered.
         */
        Index bubbleUp(Index i, const K& key) {
            Index p;
            while (i != 0 && key < this->data[p = parent(i)].first) {
                this->update(i, this->data[p]);
                i = p;
            }
            return i;
        }

        /**
         * @brief Considering only the subtree of the priority queue rooted at
         * the specified index, find the index at which an element with the
         * specified key should be inserted, rearranging the existing elements
         * to make space for the element at that index.
         *
         * @param i The index of the root of the subtree in consideration,
         * considered an empty slot when rearranging the priority queue.
         * @param key The key for the element whose insertion is considered.
         */
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

        /**
         * @brief Moves the specified element to the specified index of the
         * underlying array representation.
         *
         * @pre The specified element's value must already be part of the
         * priority queue.
         * @post The priority queue's internal data structures now contain the
         * specified element at the desired index; the element's old location
         * can be considered empty (i.e., no value lookup will resolve to the
         * old index, and the array contains stale memory at that index).
         *
         * @param i The index to which the specified element should be moved.
         * @param item The element to move to the specified index.
         */
        void update(Index i, const std::pair<K, V>& item) {
            this->data[i] = item;
            this->locator.at(item.second) = i;
        }

        /**
         * @brief Adds the specified element to the specified index of the
         * underlying array representation.
         *
         * @pre The specified element's value must not already be part of the
         * priority queue, and the specified index must be considered empty
         * (see update() for what this means).
         * @post The priority queue's internal data structures now contain the
         * specified element at the desired index.
         *
         * @param i The index at which the specified element should be add.
         * @param item The element to add at the specified index.
         */
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
