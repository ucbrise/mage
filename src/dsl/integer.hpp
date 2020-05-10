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

#include "memprog/instruction.hpp"
#include "memprog/opcode.hpp"
#include "memprog/program.hpp"
#include <cassert>
#include <cstdint>

namespace mage::dsl {
    using memprog::BitWidth;
    using memprog::VirtAddr;
    using memprog::Program;
    using memprog::OpCode;
    using memprog::invalid_vaddr;

    template <BitWidth bits>
    class Integer;

    using Bit = Integer<1>;

    template <BitWidth bits>
    class Integer {
        template <BitWidth other_bits>
        friend class Integer;

    public:
        Integer(Program& program = *Program::get_current_working_program()) : v(invalid_vaddr), p(&program) {
        }

        Integer(std::uint32_t public_constant, Program& program = *Program::get_current_working_program()) : p(&program) {
            memprog::Instruction& instr = this->p->instruction();
            instr.header.operation = OpCode::PublicConstant;
            instr.header.width = bits;
            instr.header.flags = 0;
            instr.constant.constant = public_constant;
            this->v = this->p->commit_instruction(bits);
        }

        Integer(const Integer<bits>& other) : v(other.v), p(other.p) {
        }

        void mark_input() {
            assert(this->v == invalid_vaddr);

            memprog::Instruction& instr = this->p->instruction();
            instr.header.operation = OpCode::Input;
            instr.header.width = bits;
            instr.header.flags = 0;
            this->v = this->p->commit_instruction(bits);
        }

        void mark_output() {
            this->p->mark_output(this->v, bits);
        }

        Integer<bits>& operator =(const Integer<bits>& other) {
            this->v = other.v;
            this->p = other.p;
            return *this;
        }

        Integer<bits> operator +(const Integer<bits>& other) const {
            return Integer<bits>(OpCode::IntAdd, *this, other);
        }

        Integer<bits> increment() {
            return Integer<bits>(OpCode::IntIncrement, *this);
        }

        Integer<bits>& operator ++() {
            *this = this->increment();
            return this;
        }

        Integer<bits> operator ++(int) {
            Integer<bits> old = *this;
            *this = this->increment();
            return old;
        }

        Integer<bits> operator -(const Integer<bits>& other) const {
            return Integer<bits>(OpCode::IntSub, *this, other);
        }

        Integer<bits> decrement() {
            return Integer<bits>(OpCode::IntDecrement, *this);
        }

        Integer<bits>& operator --() {
            *this = this->decrement();
            return this;
        }

        Integer<bits> operator --(int) {
            Integer<bits> old = *this;
            *this = this->decrement();
            return old;
        }

        Bit operator <(const Integer<bits>& other) const {
            return Bit(OpCode::IntLess, *this, other);
        }

        Bit operator >(const Integer<bits>& other) const {
            return other < *this;
        }

        Bit operator <=(const Integer<bits>& other) const {
            return ~(other < *this);
        }

        Bit operator >=(const Integer<bits>& other) const {
            return ~((*this) < other);
        }

        Bit operator ==(const Integer<bits>& other) const {
            return Bit(OpCode::Equal, *this, other);
        }

        Bit operator !() const {
            return Bit(OpCode::IsZero, *this);
        }

        Bit nonzero() const {
            return Bit(OpCode::NonZero, *this);
        }

        Integer<bits> operator ~() const {
            return Integer<bits>(OpCode::BitNOT, *this);
        }

        Integer<bits> operator &(const Integer<bits>& other) const {
            return Integer<bits>(OpCode::BitAND, *this, other);
        }

        Integer<bits> operator |(const Integer<bits>& other) const {
            return Integer<bits>(OpCode::BitOR, *this, other);
        }

        Integer<bits> operator ^(const Integer<bits>& other) const {
            return Integer<bits>(OpCode::BitXOR, *this, other);
        }

        template <BitWidth length>
        Integer<length> slice(BitWidth start) const {
            assert(start + length <= bits);
            return Integer<length>(this->v, start, this->p);
        }

        Bit operator [](BitWidth i) const {
            return this->slice<1>(i);
        }

        static constexpr BitWidth width() {
            return bits;
        }

        static Integer<bits> select(const Bit& selector, const Integer<bits>& arg0, const Integer<bits>& arg1) {
            return Integer<bits>(OpCode::ValueSelect, arg0, arg1, selector);
        }

        static void swap_if(const Bit& predicate, Integer<bits>& arg0, Integer<bits>& arg1) {
            assert(arg0.valid() && arg1.valid());
            Integer<bits> mask = Integer<bits>::select(predicate, arg0, arg1) ^ arg1;
            arg0 = arg0 ^ mask;
            arg1 = arg1 ^ mask;
        }

        static void comparator(Integer<bits>& arg0, Integer<bits>& arg1) {
            Integer<bits>::swap_if(arg0 > arg1, arg0, arg1);
        }

        bool valid() const {
            return this->v != invalid_vaddr;
        }

    private:
        template <BitWidth arg0_bits>
        Integer(OpCode operation, const Integer<arg0_bits>& arg0) : p(arg0.p) {
            memprog::Instruction& instr = this->p->instruction();
            instr.header.operation = operation;
            instr.header.width = arg0_bits;
            instr.header.flags = 0;
            instr.one_arg.input1 = arg0.v;
            this->v = this->p->commit_instruction(bits);
        }

        template <BitWidth arg_bits>
        Integer(OpCode operation, const Integer<arg_bits>& arg0, const Integer<arg_bits>& arg1) : p(arg0.p) {
            assert(arg0.p == arg1.p);

            memprog::Instruction& instr = this->p->instruction();
            instr.header.operation = operation;
            instr.header.width = arg_bits;
            instr.header.flags = 0;
            instr.two_args.input1 = arg0.v;
            instr.two_args.input2 = arg1.v;
            this->v = this->p->commit_instruction(bits);
        }

        template <BitWidth arg2_bits>
        Integer(OpCode operation, const Integer<bits>& arg0, const Integer<bits>& arg1, const Integer<arg2_bits>& arg2) : p(arg0.p) {
            assert(arg0.p == arg1.p);
            assert(arg0.p == arg2.p);

            memprog::Instruction& instr = this->p->instruction();
            instr.header.operation = operation;
            instr.header.width = bits;
            instr.header.flags = 0;
            instr.three_args.input1 = arg0.v;
            instr.three_args.input2 = arg1.v;
            instr.three_args.input3 = arg2.v;
            this->v = this->p->commit_instruction(bits);
        }

        Integer(VirtAddr alias_v, BitWidth offset, Program* program) : v(alias_v + offset), p(program) {
        }

        /* Address of the underlying data. */
        VirtAddr v;

        /* Program that this integer is part of. */
        Program* p;
    };
}

#endif
