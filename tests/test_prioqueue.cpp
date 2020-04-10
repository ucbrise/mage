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

#define BOOST_TEST_DYN_LINK
#include "boost/test/unit_test.hpp"
#include "boost/test/data/test_case.hpp"
#include "boost/test/data/monomorphic.hpp"

#include <algorithm>
#include <iostream>
#include <vector>
#include <sstream>

#include "util/prioqueue.hpp"

namespace bdata = boost::unit_test::data;
using mage::util::PriorityQueue;

struct VectorSample {
    std::vector<int> data;
    friend std::ostream& operator<<(std::ostream& out, const VectorSample& sample);
};

std::ostream& operator<<(std::ostream& out, const VectorSample& sample) {
    out << "{ ";
    for (auto i = sample.data.begin(); i != sample.data.end(); i++) {
        out << *i << " ";
    }
    return out << "}";
}

class RandomIntsDataset {
public:
    using sample = VectorSample;

    enum {
        arity = 1
    };

    struct iterator {
    public:
        iterator(unsigned int seed) : seedv(seed) {
            this->operator++();
        }

        VectorSample operator*() const {
            VectorSample s;
            s.data = this->sample;
            return s;
        }

        void operator++() {
            int length = rand_r(&this->seedv) % 256;
            this->sample.resize(length);
            for (int i = 0; i != length; i++) {
                this->sample[i] = i + 1;
            }
            std::random_shuffle(sample.begin(), this->sample.end());
        }
    private:
        std::vector<int> sample;
        unsigned int seedv;
    };

    RandomIntsDataset(bdata::size_t size, unsigned int seed = 12)
        : num_samples(size), seedv(seed) {
    }

    bdata::size_t size() const {
        return this->num_samples;
    }

    iterator begin() const {
        return iterator(this->seedv);
    }

private:
    bdata::size_t num_samples;
    unsigned int seedv;
};

namespace boost::unit_test::data::monomorphic {
    template <>
    struct is_dataset<RandomIntsDataset> : boost::mpl::true_ {};
}

VectorSample reverse = { .data = { 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 } };

BOOST_DATA_TEST_CASE(test_prioqueue_min, bdata::make(reverse) + RandomIntsDataset(99)) {
    std::vector<int> numbers(sample.data);
    PriorityQueue<int, int> pq;
    for (auto i = numbers.begin(); i != numbers.end(); i++) {
        pq.insert(*i, *i);
    }

    std::vector<int> popped;
    while (!pq.empty()) {
        auto res = pq.remove_min();
        BOOST_CHECK(res.first == res.second);
        popped.push_back(res.second);
    }

    std::vector<int> sorted(numbers);
    std::sort(sorted.begin(), sorted.end());
    BOOST_REQUIRE(sorted.size() == popped.size());
    for (int i = 0; i != numbers.size(); i++) {
        BOOST_CHECK(sorted[i] == popped[i]);
    }
}

BOOST_DATA_TEST_CASE(test_prioqueue_second_min, bdata::make(reverse) + RandomIntsDataset(99)) {
    std::vector<int> numbers(sample.data);
    if (numbers.size() == 0) {
        return;
    }

    PriorityQueue<int, int> pq;
    for (auto i = numbers.begin(); i != numbers.end(); i++) {
        pq.insert(*i, *i);
    }

    std::vector<int> popped;
    while (pq.size() != 1) {
        auto res = pq.remove_second_min();
        BOOST_CHECK(res.first == res.second);
        popped.push_back(res.second);
    }

    std::vector<int> sorted(numbers);
    std::sort(sorted.begin(), sorted.end());
    BOOST_REQUIRE(sorted.size() == popped.size() + 1);
    for (int i = 0; i != popped.size(); i++) {
        BOOST_CHECK(sorted[i + 1] == popped[i]);
    }
}

BOOST_DATA_TEST_CASE(test_prioqueue_decrease_key, bdata::make(reverse) + RandomIntsDataset(99)) {
    std::vector<int> numbers(sample.data);

    std::vector<int> numbers2;
    for (int i = numbers.size() / 2; i != numbers.size(); i++) {
        numbers2.push_back(numbers[i]);
    }
    numbers.resize(numbers.size() / 2);

    for (int i = 0; i != numbers.size(); i++) {
        numbers[i] = std::min(numbers[i], numbers2[i]);
    }

    PriorityQueue<int, int> pq;
    for (int i = 0; i != numbers.size(); i++) {
        pq.insert(numbers2[i], numbers[i]);
    }

    for (int i = 0; i != numbers.size(); i++) {
        pq.decrease_key(numbers[i], numbers[i]);
    }

    std::vector<int> popped;
    while (!pq.empty()) {
        auto res = pq.remove_min();
        BOOST_CHECK(res.first == res.second);
        popped.push_back(res.second);
    }

    std::vector<int> sorted(numbers);
    std::sort(sorted.begin(), sorted.end());
    BOOST_REQUIRE(sorted.size() == popped.size());
    for (int i = 0; i != numbers.size(); i++) {
        BOOST_CHECK(sorted[i] == popped[i]);
    }
}
