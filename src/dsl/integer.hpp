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

    template <BitWidth bits, typename Placer, Program<Placer>** p>
    class Integer;

    template<typename Placer, Program<Placer>** p>
    using Bit = Integer<1, Placer, p>;

    enum class Party : std::uint32_t {
        Garbler = 0,
        Evaluator = 1,
    };

    template <BitWidth bits, typename Placer, Program<Placer>** p>
    class Integer {
        template <BitWidth other_bits, typename OtherPlacer, Program<OtherPlacer>** other_p>
        friend class Integer;

        static_assert(bits > 0);

    public:
        Integer() : v(invalid_vaddr), sliced(false) {
        }

        Integer(std::uint32_t public_constant) : sliced(false) {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::PublicConstant;
            instr.header.width = bits;
            instr.header.flags = 0;
            instr.constant.constant = public_constant;
            this->v = (*p)->commit_instruction(bits);
        }

        Integer(const Integer<bits, Placer, p>& other) = delete;

        Integer(Integer<bits, Placer, p>&& other) : v(other.v), sliced(other.sliced) {
            other.v = invalid_vaddr;
            other.sliced = false;
        }

        ~Integer() {
            this->recycle();
        }

        void mark_input(enum Party party) {
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

        Integer<bits, Placer, p>& operator =(const Integer<bits, Placer, p>& other) = delete;

        Integer<bits, Placer, p>& operator =(Integer<bits, Placer, p>&& other) {
            this->recycle();

            this->v = other.v;
            this->sliced = other.sliced;

            other.v = invalid_vaddr;
            other.sliced = false;

            return *this;
        }

        Integer<bits, Placer, p> operator +(const Integer<bits, Placer, p>& other) const {
            return Integer<bits, Placer, p>(OpCode::IntAdd, *this, other);
        }

        Integer<bits, Placer, p> increment() {
            return Integer<bits, Placer, p>(OpCode::IntIncrement, *this);
        }

        Integer<bits, Placer, p>& operator ++() {
            *this = this->increment();
            return this;
        }

        Integer<bits, Placer, p> operator ++(int) {
            Integer<bits, Placer, p> old = *this;
            *this = this->increment();
            return old;
        }

        Integer<bits, Placer, p> operator -(const Integer<bits, Placer, p>& other) const {
            return Integer<bits, Placer, p>(OpCode::IntSub, *this, other);
        }

        Integer<bits, Placer, p> decrement() {
            return Integer<bits, Placer, p>(OpCode::IntDecrement, *this);
        }

        Integer<bits, Placer, p>& operator --() {
            *this = this->decrement();
            return this;
        }

        Integer<bits, Placer, p> operator --(int) {
            Integer<bits, Placer, p> old = *this;
            *this = this->decrement();
            return old;
        }

        Bit<Placer, p> operator <(const Integer<bits, Placer, p>& other) const {
            return Bit<Placer, p>(OpCode::IntLess, *this, other);
        }

        Bit<Placer, p> operator >(const Integer<bits, Placer, p>& other) const {
            return other < *this;
        }

        Bit<Placer, p> operator <=(const Integer<bits, Placer, p>& other) const {
            return ~(other < *this);
        }

        Bit<Placer, p> operator >=(const Integer<bits, Placer, p>& other) const {
            return ~((*this) < other);
        }

        Bit<Placer, p> operator ==(const Integer<bits, Placer, p>& other) const {
            return Bit<Placer, p>(OpCode::Equal, *this, other);
        }

        Bit<Placer, p> operator !() const {
            return Bit<Placer, p>(OpCode::IsZero, *this);
        }

        Bit<Placer, p> nonzero() const {
            return Bit<Placer, p>(OpCode::NonZero, *this);
        }

        Integer<bits, Placer, p> operator ~() const {
            return Integer<bits, Placer, p>(OpCode::BitNOT, *this);
        }

        Integer<bits, Placer, p> operator &(const Integer<bits, Placer, p>& other) const {
            return Integer<bits, Placer, p>(OpCode::BitAND, *this, other);
        }

        Integer<bits, Placer, p> operator |(const Integer<bits, Placer, p>& other) const {
            return Integer<bits, Placer, p>(OpCode::BitOR, *this, other);
        }

        Integer<bits, Placer, p> operator ^(const Integer<bits, Placer, p>& other) const {
            return Integer<bits, Placer, p>(OpCode::BitXOR, *this, other);
        }

        template <BitWidth length>
        Integer<length, Placer, p> slice(BitWidth start) const {
            assert(start + length <= bits);
            return Integer<length, Placer, p>(this->v, start);
        }

        Bit<Placer, p> operator [](BitWidth i) const {
            return this->slice<1>(i);
        }

        static constexpr BitWidth width() {
            return bits;
        }

        static Integer<bits, Placer, p> select(const Bit<Placer, p>& selector, const Integer<bits, Placer, p>& arg0, const Integer<bits, Placer, p>& arg1) {
            return Integer<bits, Placer, p>(OpCode::ValueSelect, arg0, arg1, selector);
        }

        static void swap_if(const Bit<Placer, p>& predicate, Integer<bits, Placer, p>& arg0, Integer<bits, Placer, p>& arg1) {
            assert(arg0.valid() && arg1.valid());
            Integer<bits, Placer, p> mask = Integer<bits, Placer, p>::select(predicate, arg0, arg1) ^ arg1;
            arg0 = arg0 ^ mask;
            arg1 = arg1 ^ mask;
        }

        static void comparator(Integer<bits, Placer, p>& arg0, Integer<bits, Placer, p>& arg1) {
            Integer<bits, Placer, p>::swap_if(arg0 > arg1, arg0, arg1);
        }

        bool valid() const {
            return this->v != invalid_vaddr;
        }

    private:
        void recycle() {
            if (this->sliced) {
                this->sliced = false;
                this->v = invalid_vaddr;
            } else if (this->v != invalid_vaddr) {
                (*p)->recycle(this->v, bits);
                this->v = invalid_vaddr;
            }
        }

        template <BitWidth arg0_bits>
        Integer(OpCode operation, const Integer<arg0_bits, Placer, p>& arg0) : sliced(false) {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = operation;
            instr.header.width = arg0_bits;
            instr.header.flags = 0;
            instr.one_arg.input1 = arg0.v;
            this->v = (*p)->commit_instruction(bits);
        }

        template <BitWidth arg_bits>
        Integer(OpCode operation, const Integer<arg_bits, Placer, p>& arg0, const Integer<arg_bits, Placer, p>& arg1) : sliced(false) {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = operation;
            instr.header.width = arg_bits;
            instr.header.flags = 0;
            instr.two_args.input1 = arg0.v;
            instr.two_args.input2 = arg1.v;
            this->v = (*p)->commit_instruction(bits);
        }

        template <BitWidth arg2_bits>
        Integer(OpCode operation, const Integer<bits, Placer, p>& arg0, const Integer<bits, Placer, p>& arg1, const Integer<arg2_bits, Placer, p>& arg2) : sliced(false) {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = operation;
            instr.header.width = bits;
            instr.header.flags = 0;
            instr.three_args.input1 = arg0.v;
            instr.three_args.input2 = arg1.v;
            instr.three_args.input3 = arg2.v;
            this->v = (*p)->commit_instruction(bits);
        }

        Integer(VirtAddr alias_v, BitWidth offset) : v(alias_v + offset), sliced(true) {
        }

        /* Address of the underlying data. */
        VirtAddr v;

        /* Is the address sliced? */
        bool sliced;
    };
}

#endif
