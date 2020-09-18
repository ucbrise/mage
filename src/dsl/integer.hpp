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

#include "instruction.hpp"
#include "memprog/program.hpp"
#include "opcode.hpp"
#include <cassert>
#include <cstdint>

namespace mage::dsl {
    using memprog::Program;

    template <BitWidth bits, bool sliced, typename Placer, Program<Placer>** p>
    class Integer;

    template<bool sliced, typename Placer, Program<Placer>** p>
    using Bit = Integer<1, sliced, Placer, p>;

    enum class Party : std::uint32_t {
        Garbler = 0,
        Evaluator = 1,
    };

    template <BitWidth bits, bool sliced, typename Placer, Program<Placer>** p>
    class Integer {
        template <BitWidth other_bits, bool other_sliced, typename OtherPlacer, Program<OtherPlacer>** other_p>
        friend class Integer;

        static_assert(bits > 0);

    public:
        Integer() : v(invalid_vaddr) {
        }

        Integer(std::uint32_t public_constant) {
            static_assert(!sliced);

            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::PublicConstant;
            instr.header.width = bits;
            instr.header.flags = 0;
            instr.constant.constant = public_constant;
            this->v = (*p)->commit_instruction(bits);
        }

        Integer(const Integer<bits, sliced, Placer, p>& other) = delete;

        Integer(Integer<bits, sliced, Placer, p>&& other) : v(other.v) {
            other.v = invalid_vaddr;
        }

        ~Integer() {
            this->recycle();
        }

        void mark_input(enum Party party) {
            assert(!sliced);
            this->recycle();

            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::Input;
            instr.header.width = bits;
            instr.header.flags = (party == Party::Garbler) ? 0 : FlagEvaluatorInput;
            this->v = (*p)->commit_instruction(bits);
        }

        void mark_output() {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::Output;
            instr.header.width = bits;
            instr.header.flags = 0;
            instr.header.output = this->v;
            (*p)->commit_instruction(0);
        }

        Integer<bits, sliced, Placer, p>& operator =(const Integer<bits, sliced, Placer, p>& other) = delete;

        Integer<bits, sliced, Placer, p>& operator =(Integer<bits, sliced, Placer, p>&& other) {
            this->recycle();
            this->v = other.v;
            other.v = invalid_vaddr;
            return *this;
        }

        template <bool other_sliced>
        Integer<bits, false, Placer, p> operator +(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Integer<bits, false, Placer, p>(OpCode::IntAdd, *this, other);
        }

        Integer<bits, false, Placer, p> increment() {
            return Integer<bits, false, Placer, p>(OpCode::IntIncrement, *this);
        }

        /*
        These operations do not appear to be correctly implemented. They don't
        increment in place; they create a new integer that is incremented and
        reassign the address. I suppose that's fine, but what if the integer
        is sliced (meaning someone else can observer the underlying virtual
        address)? It's not clear what the semantics should be.

        Integer<bits, Placer, p>& operator ++() {
            *this = this->increment();
            return this;
        }

        Integer<bits, Placer, p> operator ++(int) {
            Integer<bits, Placer, p> old = *this;
            *this = this->increment();
            return old;
        }
        */

        template <bool other_sliced>
        Integer<bits, false, Placer, p> operator -(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Integer<bits, false, Placer, p>(OpCode::IntSub, *this, other);
        }

        Integer<bits, false, Placer, p> decrement() {
            return Integer<bits, false, Placer, p>(OpCode::IntDecrement, *this);
        }

        /*
        Same problem as prefix/postfix increment

        Integer<bits, Placer, p>& operator --() {
            *this = this->decrement();
            return this;
        }

        Integer<bits, Placer, p> operator --(int) {
            Integer<bits, Placer, p> old = *this;
            *this = this->decrement();
            return old;
        }
        */

        template <bool other_sliced>
        Bit<false, Placer, p> operator <(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Bit<false, Placer, p>(OpCode::IntLess, *this, other);
        }

        template <bool other_sliced>
        Bit<false, Placer, p> operator >(const Integer<bits, other_sliced, Placer, p>& other) const {
            return other < *this;
        }

        template <bool other_sliced>
        Bit<false, Placer, p> operator <=(const Integer<bits, other_sliced, Placer, p>& other) const {
            return ~(other < *this);
        }

        template <bool other_sliced>
        Bit<false, Placer, p> operator >=(const Integer<bits, other_sliced, Placer, p>& other) const {
            return ~((*this) < other);
        }

        template <bool other_sliced>
        Bit<false, Placer, p> operator ==(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Bit<false, Placer, p>(OpCode::Equal, *this, other);
        }

        Bit<false, Placer, p> operator !() const {
            return Bit<false, Placer, p>(OpCode::IsZero, *this);
        }

        Bit<false, Placer, p> nonzero() const {
            return Bit<false, Placer, p>(OpCode::NonZero, *this);
        }

        Integer<bits, false, Placer, p> operator ~() const {
            return Integer<bits, false, Placer, p>(OpCode::BitNOT, *this);
        }

        template <bool other_sliced>
        Integer<bits, false, Placer, p> operator &(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Integer<bits, false, Placer, p>(OpCode::BitAND, *this, other);
        }

        template <bool other_sliced>
        Integer<bits, false, Placer, p> operator |(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Integer<bits, false, Placer, p>(OpCode::BitOR, *this, other);
        }

        template <bool other_sliced>
        Integer<bits, false, Placer, p> operator ^(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Integer<bits, false, Placer, p>(OpCode::BitXOR, *this, other);
        }

        template <BitWidth length>
        Integer<length, true, Placer, p> slice(BitWidth start) const {
            assert(start + length <= bits);
            return Integer<length, true, Placer, p>(this->v, start);
        }

        Bit<true, Placer, p> operator [](BitWidth i) const {
            return this->slice<1>(i);
        }

        static constexpr BitWidth width() {
            return bits;
        }

        template <bool selector_sliced, bool arg0_sliced, bool arg1_sliced>
        static Integer<bits, false, Placer, p> select(const Bit<selector_sliced, Placer, p>& selector, const Integer<bits, arg0_sliced, Placer, p>& arg0, const Integer<bits, arg1_sliced, Placer, p>& arg1) {
            return Integer<bits, false, Placer, p>(OpCode::ValueSelect, arg0, arg1, selector);
        }

        template <bool predicate_sliced>
        static void swap_if(const Bit<predicate_sliced, Placer, p>& predicate, Integer<bits, false, Placer, p>& arg0, Integer<bits, false, Placer, p>& arg1) {
            assert(arg0.valid() && arg1.valid());
            Integer<bits, false, Placer, p> mask = Integer<bits, false, Placer, p>::select(predicate, arg0, arg1) ^ arg1;
            arg0 = arg0 ^ mask;
            arg1 = arg1 ^ mask;
        }

        static void comparator(Integer<bits, false, Placer, p>& arg0, Integer<bits, false, Placer, p>& arg1) {
            Integer<bits, false, Placer, p>::swap_if(arg0 > arg1, arg0, arg1);
        }

        bool valid() const {
            return this->v != invalid_vaddr;
        }

    private:
        void recycle() {
            if constexpr (sliced) {
                this->v = invalid_vaddr;
            } else if (this->v != invalid_vaddr) {
                (*p)->recycle(this->v, bits);
                this->v = invalid_vaddr;
            }
        }

        template <BitWidth arg0_bits, bool arg0_sliced>
        Integer(OpCode operation, const Integer<arg0_bits, arg0_sliced, Placer, p>& arg0) {
            static_assert(!sliced);
            Instruction& instr = (*p)->instruction();
            instr.header.operation = operation;
            instr.header.width = arg0_bits;
            instr.header.flags = 0;
            instr.one_arg.input1 = arg0.v;
            this->v = (*p)->commit_instruction(bits);
        }

        template <BitWidth arg_bits, bool arg0_sliced, bool arg1_sliced>
        Integer(OpCode operation, const Integer<arg_bits, arg0_sliced, Placer, p>& arg0, const Integer<arg_bits, arg1_sliced, Placer, p>& arg1) {
            static_assert(!sliced);
            Instruction& instr = (*p)->instruction();
            instr.header.operation = operation;
            instr.header.width = arg_bits;
            instr.header.flags = 0;
            instr.two_args.input1 = arg0.v;
            instr.two_args.input2 = arg1.v;
            this->v = (*p)->commit_instruction(bits);
        }

        template <BitWidth arg2_bits, bool arg0_sliced, bool arg1_sliced, bool arg2_sliced>
        Integer(OpCode operation, const Integer<bits, arg0_sliced, Placer, p>& arg0, const Integer<bits, arg1_sliced, Placer, p>& arg1, const Integer<arg2_bits, arg2_sliced, Placer, p>& arg2) {
            static_assert(!sliced);
            Instruction& instr = (*p)->instruction();
            instr.header.operation = operation;
            instr.header.width = bits;
            instr.header.flags = 0;
            instr.three_args.input1 = arg0.v;
            instr.three_args.input2 = arg1.v;
            instr.three_args.input3 = arg2.v;
            this->v = (*p)->commit_instruction(bits);
        }

        Integer(VirtAddr alias_v, BitWidth offset) : v(alias_v + offset) {
            static_assert(sliced);
        }

        /* Address of the underlying data. */
        VirtAddr v;
    };
}

#endif
