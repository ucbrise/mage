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

#ifndef MAGE_DSL_INTEGER_HPP_
#define MAGE_DSL_INTEGER_HPP_

#include "dsl/graph.hpp"
#include <cassert>
#include <cstdint>

namespace mage::dsl {
    template <BitWidth bits, Graph* g>
    class Integer;

    template <Graph* g>
    using Bit = Integer<1, g>;

    template <BitWidth bits, Graph* g>
    class Integer {
        template <BitWidth other_bits, Graph* other_graph>
        friend class Integer;

    public:
        Integer() : v(invalid_vertex), bit_offset(0) {
        }

        Integer(std::uint32_t public_constant) : bit_offset(0) {
            this->v = g->new_vertex(Operation::PublicConstant, bits, public_constant);
        }

        Integer(const Integer<bits, g>& other) : v(other.v) {
        }

        void mark_input() {
            assert(this->v == invalid_vertex);
            this->v = g->new_vertex(Operation::Input, bits);
        }

        void mark_output() {
            g->mark_output(this->v);
        }

        Integer<bits, g>& operator =(const Integer<bits, g>& other) {
            this->v = other.v;
            this->bit_offset = other.bit_offset;
            return *this;
        }

        Integer<bits, g> operator +(const Integer<bits, g>& other) const {
            return Integer<bits, g>(Operation::IntAdd, *this, other);
        }

        Integer<bits, g> increment() {
            return Integer<bits, g>(Operation::IntIncrement, *this);
        }

        Integer<bits, g>& operator ++() {
            *this = this->increment();
            return this;
        }

        Integer<bits, g> operator ++(int) {
            Integer<bits, g> old = *this;
            *this = this->increment();
            return old;
        }

        Integer<bits, g> operator -(const Integer<bits, g>& other) const {
            return Integer<bits, g>(Operation::IntSub, *this, other);
        }

        Integer<bits, g> decrement() {
            return Integer<bits, g>(Operation::IntDecrement, *this);
        }

        Integer<bits, g>& operator --() {
            *this = this->decrement();
            return this;
        }

        Integer<bits, g> operator --(int) {
            Integer<bits, g> old = *this;
            *this = this->decrement();
            return old;
        }

        Bit<g> operator <(const Integer<bits, g>& other) const {
            return Bit<g>(Operation::IntLess, *this, other);
        }

        Bit<g> operator >(const Integer<bits, g>& other) const {
            return other < *this;
        }

        Bit<g> operator <=(const Integer<bits, g>& other) const {
            return ~(other < *this);
        }

        Bit<g> operator >=(const Integer<bits, g>& other) const {
            return ~((*this) < other);
        }

        Bit<g> operator ==(const Integer<bits, g>& other) const {
            return Bit<g>(Operation::Equal, *this, other);
        }

        Bit<g> operator !() const {
            return Bit<g>(Operation::IsZero, *this);
        }

        Bit<g> nonzero() const {
            return Bit<g>(Operation::NonZero, *this);
        }

        Integer<bits, g> operator ~() const {
            return Integer<bits, g>(Operation::BitNOT, *this);
        }

        Integer<bits, g> operator &(const Integer<bits, g>& other) const {
            return Integer<bits, g>(Operation::BitAND, *this, other);
        }

        Integer<bits, g> operator |(const Integer<bits, g>& other) const {
            return Integer<bits, g>(Operation::BitOR, *this, other);
        }

        Integer<bits, g> operator ^(const Integer<bits, g>& other) const {
            return Integer<bits, g>(Operation::BitXOR, *this, other);
        }

        template <BitWidth length>
        Integer<length, g> slice(BitOffset start) const {
            assert(start + length <= bits);
            BitOffset new_offset = this->bit_offset + start;
            assert(new_offset >= start); // check overflow of BitOffset
            return Integer<length, g>(this->v, new_offset);
        }

        Bit<g> operator [](BitOffset i) const {
            return this->slice<1>(i);
        }

        static constexpr BitWidth width() {
            return bits;
        }

        static Integer<bits, g> select(const Bit<g>& selector, const Integer<bits, g>& arg0, const Integer<bits, g>& arg1) {
            return Integer<bits, g>(Operation::ValueSelect, arg0, arg1, selector);
        }

        static void swap_if(const Bit<g>& predicate, Integer<bits, g>& arg0, Integer<bits, g>& arg1) {
            assert(arg0.valid() && arg1.valid());
            Integer<bits, g> mask = Integer<bits, g>::select(predicate, arg0, arg1) ^ arg1;
            arg0 = arg0 ^ mask;
            arg1 = arg1 ^ mask;
        }

        static void comparator(Integer<bits, g>& arg0, Integer<bits, g>& arg1) {
            Integer<bits, g>::swap_if(arg0 > arg1, arg0, arg1);
        }

        bool valid() const {
            return this->v != invalid_vertex;
        }

    private:
        template <BitWidth arg0_bits>
        Integer(Operation operation, const Integer<arg0_bits, g>& arg0) : bit_offset(0) {
            this->v = g->new_vertex(operation, bits, arg0.v, arg0.bit_offset);
        }

        template <BitWidth arg_bits>
        Integer(Operation operation, const Integer<arg_bits, g>& arg0, const Integer<arg_bits, g>& arg1) : bit_offset(0) {
            this->v = g->new_vertex(operation, bits, arg0.v, arg0.bit_offset, arg1.v, arg1.bit_offset);
        }

        template <BitWidth arg2_bits>
        Integer(Operation operation, const Integer<bits, g>& arg0, const Integer<bits, g>& arg1, const Integer<arg2_bits, g>& arg2) : bit_offset(0) {
            this->v = g->new_vertex(operation, bits, arg0.v, arg0.bit_offset, arg1.v, arg1.bit_offset, arg2.v);
        }

        Integer(VertexID alias_v, BitWidth offset) : v(alias_v), bit_offset(0) {
        }

        /* ID of the underlying vertex in the computation graph */
        VertexID v;
        BitOffset bit_offset;
    };
}

#endif
