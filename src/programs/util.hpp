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

#ifndef MAGE_PROGRAMS_UTIL_HPP_
#define MAGE_PROGRAMS_UTIL_HPP_

#include "dsl/integer.hpp"

using namespace mage::dsl;

namespace mage::programs {
    using DefaultPlacer = memprog::BinnedPlacer;
    constexpr Program<DefaultPlacer>** default_program = &program_ptr;

    template <BitWidth bits>
    using Integer = mage::dsl::Integer<bits, false, memprog::BinnedPlacer, default_program>;

    template <BitWidth bits>
    using IntSlice = mage::dsl::Integer<bits, true, memprog::BinnedPlacer, default_program>;

    using Bit = Integer<1>;
    using BitSlice = IntSlice<1>;

    template <BitWidth width = 8>
    Integer<2 * width> dot_product(Integer<width>* vector_a, Integer<width>* vector_b, std::size_t length) {
        assert(length != 0);
        Integer<2 * width> total = vector_a[0] * vector_b[0];
        for (std::size_t i = 1; i != length; i++) {
            total = total + (vector_a[i] * vector_b[i]);
        }
        return total;
    }

    template <BitWidth key_width, BitWidth record_width>
    struct Record {
        Integer<record_width> data;

        IntSlice<key_width> get_key() {
            return this->data.template slice<key_width>(0);
        }

        IntSlice<record_width> get_record() {
            return this->data.template slice<record_width>(key_width);
        }

        static void comparator(Record<key_width, record_width>& arg0, Record<key_width, record_width>& arg1) {
            IntSlice<key_width> key0 = arg0.get_key();
            IntSlice<key_width> key1 = arg1.get_key();
            Bit predicate = key0 > key1;
            Integer<record_width>::swap_if(predicate, arg0.data, arg1.data);
        }

        void buffer_send(WorkerID to) {
            this->data.buffer_send(to);
        }

        static void finish_send(WorkerID to) {
            Integer<record_width>::finish_send(to);
        }

        void post_receive(WorkerID from) {
            this->data.post_receive(from);
        }

        static void finish_receive(WorkerID from) {
            Integer<record_width>::finish_receive(from);
        }
    };
}

#endif
