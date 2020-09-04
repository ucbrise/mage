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

#include <cstdint>
#include <vector>

#include "util/circularbuffer.hpp"

namespace bdata = boost::unit_test::data;
using mage::util::CircularBuffer;

constexpr const std::size_t circbuf_capacity_shift = 6;
constexpr const std::size_t circbuf_capacity = 1 << circbuf_capacity_shift;
constexpr const std::uint64_t num_iterations = 100;

BOOST_DATA_TEST_CASE(test_circbuffer_wrap, bdata::xrange(circbuf_capacity)) {
    CircularBuffer<std::uint64_t> cb(circbuf_capacity_shift);

    std::uint64_t counter = 0;
    for (std::uint64_t i = 0; i != num_iterations; i++) {
        std::vector<std::uint64_t> x(sample);
        for (std::uint64_t i = 0; i != sample; i++) {
            x[i] = i;
        }
        cb.write_unchecked(x.data(), sample);

        std::vector<std::uint64_t> y(sample);
        cb.read_unchecked(y.data(), sample);

        BOOST_REQUIRE(x.size() == sample);
        BOOST_REQUIRE(x.size() == y.size());
        for (std::uint64_t k = 0; k != x.size(); k++) {
            BOOST_CHECK_MESSAGE(x[k] == y[k], "x[" << k << "] is " << x[k] << ", but y[" << k << "] is " << y[k]);
        }
    }
}
