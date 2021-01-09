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
 * @file dsl/integer.hpp
 * @brief Integer DSL for writing programs for MAGE.
 */

#ifndef MAGE_DSL_INTEGER_HPP_
#define MAGE_DSL_INTEGER_HPP_

#include <cassert>
#include <cstdint>
#include "instruction.hpp"
#include "memprog/program.hpp"
#include "addr.hpp"
#include "opcode.hpp"

namespace mage::dsl {
    using memprog::Program;

    template <BitWidth bits, bool sliced, typename Placer, Program<Placer>** p>
    class Integer;

    /**
     * @brief The Bit is an alias of Integer where the length is fixed as 1.
     *
     * All operations on Integers also work on the Bit type.
     */
    template<bool sliced, typename Placer, Program<Placer>** p>
    using Bit = Integer<1, sliced, Placer, p>;

    /**
     * @brief Used to indicate which party provides input in
     * Integer::mark_input().
     */
    enum class Party : std::uint32_t {
        Garbler = 0,
        Evaluator = 1,
    };

    /* TODO: reduce placement allocations by overloading on ref-qualifiers? */

    /**
     * @brief An Integer is a fixed-width sequence of bits in a bit-addressed
     * MAGE-virtual address space representing an integer.
     *
     * An Integer can be understood as storing a pointer to memory in a
     * MAGE-virtual address space. If the Integer has ownership of that memory,
     * @p sliced is false. If not, some other Integer has ownership of that
     * memory, we saw that this Integer is a slice of that Integer, and
     * @p is true. If @p sliced is false for an Integer, it deallocates its
     * underlying MAGE-virtual memory when it goes out of scope, or if an
     * operation is performed that changes the MAGE-virtual memory that it
     * points to.
     *
     * In some cases, an Integer's MAGE-virtual memory may not be allocated.
     * For example, this will happen if the default constructor is used to
     * create an Integer. It will also happen if it is used to move-construct
     * or move-assign to another Integer. An Integer in such a state is said
     * to be invalid.
     *
     * When an operation is performed using Integer objects, this class takes
     * the following steps: (1) it uses @p p to allocate space for the
     * resulting integer in the MAGE-virtual address space, (2) it initializes
     * and returns a new Integer object representing the newly allocated
     * space in the MAGE-virtual address space, and (3) it emits an instruction
     * using @p p to perform the operation, reading its arguments from the
     * spaces in the MAGE-virtual address space corresponding to the Integer
     * arguments and storing the result in the newly allocated space in the
     * MAGE-virtual address space.
     *
     * The documentation below generally describes the effect that the
     * functions have in the program. In reality, when the functions below are
     * executed, they emit instructions that perform the described actions.
     * Specifically, statements about an Integer's "value" refer to its
     * semantic behavior in the program, and statements about its "memory"
     * refers to any aliasing behavior that the programmer should be aware of.
     *
     * @tparam bits The width of this Integer in bits.
     * @tparam sliced If true, this object does not have ownership of its
     * underlying space in the MAGE-virtual address space; the underlying space
     * is owned by a single non-sliced Integer. One that non-sliced Integer
     * goes out of scope, that MAGE-virtual memory is deallocated and any
     * sliced Integers still referencing that memory should no longer be used.
     * @tparam Placer Type of the placement algorithm used to allocate and
     * deallocate memory in the MAGE-virtual address space.
     * @tparam p Double pointer to the program object with access to MAGE's
     * placement module and to the intermediate bytecode being written.
     */
    template <BitWidth bits, bool sliced, typename Placer, Program<Placer>** p>
    class Integer {
        template <BitWidth other_bits, bool other_sliced, typename OtherPlacer, Program<OtherPlacer>** other_p>
        friend class Integer;

        static_assert(bits > 0);

    public:
        /**
         * @brief Creates an invalid Integer, with no underlying memory. Before
         * using this Integer, one should call mark_input() or another
         * operation that allocates memory for this Integer before using it.
         */
        Integer() : v(invalid_vaddr) {
        }

        /**
         * @brief Creates an Integer, allocates fresh memory for it, and
         * and initializes its value to the provided constant.
         *
         * @sa mutate_to_constant()
         *
         * @param public_constant The value to which to initialize the Integer.
         */
        Integer(std::uint32_t public_constant) {
            static_assert(!sliced);

            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::PublicConstant;
            instr.header.width = bits;
            instr.header.flags = 0;
            instr.constant.constant = public_constant;
            this->v = (*p)->commit_instruction(bits);
        }

        /**
         * @brief Integer is not copy-constructible.
         *
         * @sa mutate()
         */
        Integer(const Integer<bits, sliced, Placer, p>& other) = delete;

        /**
         * @brief Move-constructs an Integer, setting its value to that of the
         * specified Integer.
         *
         * Instead of copying the value from one Integer to another, ownership
         * of the underlying memory is transferred from the provided Integer to
         * this one. Thus, this operation has zero runtime cost.
         *
         * This implies that any aliasing of the specified Integer is
         * preserved. For example, if a slice of an Integer is obtained (see
         * slice()) and then it is used as an argument to a move constructor
         * the newly created Integer will be aliased with the original
         * Integer's slices.
         *
         * After an Integer is used as an argument to the move constructor,
         * it is uninitialized, as if it were constructed with the default
         * constructor.
         *
         * @param other The Integer to whose value this Integer should be set.
         */
        Integer(Integer<bits, sliced, Placer, p>&& other) : v(other.v) {
            other.v = invalid_vaddr;
        }

        /**
         * @brief Deallocates the underlying memory for this Integer, unless
         * this Integer is sliced.
         */
        ~Integer() {
            this->recycle();
        }

        /**
         * @brief Overwrites the value of this Integer with data read from the
         * program's input.
         *
         * @param party The party whose input to read (only applicable if
         * the underlying protocol supports multiple parties).
         */
        void mark_input(enum Party party) {
            static_assert(!sliced);
            this->recycle();

            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::Input;
            instr.header.width = bits;
            instr.header.flags = (party == Party::Garbler) ? 0 : FlagEvaluatorInput;
            this->v = (*p)->commit_instruction(bits);
        }

        /**
         * @brief Writes the value of this Integer to the program's output.
         */
        void mark_output() {
            assert(this->valid());

            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::Output;
            instr.header.width = bits;
            instr.header.flags = 0;
            instr.header.output = this->v;
            (*p)->commit_instruction(0);
        }

        /**
         * @brief Integer is not copy-assignable.
         *
         * @sa mutate()
         */
        Integer<bits, sliced, Placer, p>& operator =(const Integer<bits, sliced, Placer, p>& other) = delete;

        /**
         * @brief Move-assigns an Integer, setting its value to that of the
         * specified Integer.
         *
         * All the details of the move constructor also apply here.
         *
         * @param other The Integer to whose value this Integer should be set.
         */
        Integer<bits, sliced, Placer, p>& operator =(Integer<bits, sliced, Placer, p>&& other) {
            this->recycle();
            this->v = other.v;
            other.v = invalid_vaddr;
            return *this;
        }

        /**
         * @brief Sets the value of this Integer to the provided constant,
         * writing to the underlying memory as opposed to allocating new memory
         * (if possible).
         *
         * A similar operation would be to invoke the Integer constructor to
         * initialize an Integer to the provided value, and then use
         * move-assignment to set this Integer to that value. This function is
         * different, in that it writes the specified constant to this
         * Integer's existing memory, preserving aliasing relationships of
         * this Integer. In contrast, constructing a new Integer and then
         * using move assignment replaces the underlying memory used by this
         * Integer, breaking any aliasing relationships.
         *
         * For example, suppose one obtains a slice of an Integer that refers
         * to bits 0 to 2 of that integer, and uses calls this function on
         * the original Integer with the value 259. The value of the slice is
         * now 3, updated by the change to the original Integer. Conversely,
         * calling this function on the slice would update the value of the
         * original Integer as well. The same logic applies to mutate(),
         * below.
         *
         * @sa mutate()
         *
         * @param public_constant The value to store in this Integer.
         */
        void mutate_to_constant(std::uint32_t public_constant) {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::PublicConstant;
            instr.header.width = bits;
            instr.header.flags = 0;
            instr.constant.constant = public_constant;
            if (this->valid()) {
                instr.header.output = this->v;
                (*p)->commit_instruction(0);
            } else if constexpr (!sliced) {
                this->v = (*p)->commit_instruction(bits);
            } else {
                std::cerr << "Mutating uninitialized sliced Integer" << std::endl;
                std::abort();
            }
        }

        /**
         * @brief Copies the value of the specified Integer into this Integer,
         * allocating fresh memory for this Integer, if necessary.
         *
         * Unlike the move constructor or move assignment, ownership of the
         * underlying memory is not transferred to this Integer. Instead, the
         * value of the specified Integer is copied to this one, which has a
         * nonzero runtime cost. If this is an invalid Integer (no
         * underlying memory is allocated), a fresh block of memory is
         * allocated for this Integer, this Integer is initialized to use that
         * memory, and the specified Integer's value is copied to that memory.
         *
         * Unlike the move constructor, aliasing is not carried through. For
         * example, if an Integer x is sliced, and then mutate() is called for
         * this Integer with x as an argument, this Integer is not aliased with
         * x or any slices of x. Integer x and this Integer remain completely
         * distinct.
         *
         * Logically, the behavior here is similar to what one may expect to
         * see in a copy constructor or copy assignment operator. The copy
         * constructor and copy assignment operators are deleted to avoid
         * "hiding" copy operations from the programmer. Forcing the programmer
         * to call mutate() makes the copies more visible in the code.
         *
         * @sa mutate_to_constant()
         *
         * @param other The Integer object whose value to copy into this one.
         */
        template <BitWidth other_bits, bool other_sliced>
        void mutate(const Integer<other_bits, other_sliced, Placer, p>& other) {
            static_assert(other_bits <= bits);

            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::Copy;
            instr.header.width = other_bits;
            instr.header.flags = 0;
            instr.one_arg.input1 = other.v;
            if (this->valid()) {
                instr.header.output = this->v;
                (*p)->commit_instruction(0);
            } else if constexpr (!sliced) {
                this->v = (*p)->commit_instruction(bits);
            } else {
                std::cerr << "Mutating uninitialized sliced Integer" << std::endl;
                std::abort();
            }

            if constexpr (other_bits != bits) {
                this->slice<bits - other_bits>(other_bits).mutate_to_constant(0);
            }
        }

        /**
         * @brief Sends the contents of this Integer to the specified worker.
         *
         * The data is enqueued into a buffer; it may not be sent immediately.
         * To force the data to be sent, use finish_send().
         *
         * @param to The ID of the worker to which the data should be sent.
         */
        void buffer_send(WorkerID to) const {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::NetworkBufferSend;
            instr.header.width = bits;
            instr.header.flags = 0;
            instr.header.output = this->v;
            instr.constant.constant = to;
            (*p)->commit_instruction(0);
        }

        /**
         * @brief Overwrites the value of this Integer with data received from
         * the specified worker, allocating the underlying memory for this
         * Integer if it was previously invalid.
         *
         * Once this function returns, the receive has been initiated but has
         * not necessarily completed. To wait until the desired data has been
         * received from the worker, use finish_receive().
         *
         * @param from The ID of the worker from which data should be received.
         */
        void post_receive(WorkerID from) {
            static_assert(!sliced);
            this->recycle();

            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::NetworkPostReceive;
            instr.header.width = bits;
            instr.header.flags = 0;
            instr.constant.constant = from;
            if (this->valid()) {
                instr.header.output = this->v;
                (*p)->commit_instruction(0);
            } else {
                this->v = (*p)->commit_instruction(bits);
            }
        }

        /**
         * @brief Force any pending data to the specified worker to be sent.
         *
         * @param to The worker whose pending data is sent.
         */
        static void finish_send(WorkerID to) {
            (*p)->finish_send(to);
        }

        /**
         * @brief Wait for any outstanding receive operations from the
         * specified worker to complete.
         *
         * @param from The worker from whom any pending receive operations
         * complete.
         */
        static void finish_receive(WorkerID from) {
            (*p)->finish_receive(from);
        }

        /**
         * @brief Computes the sum of this Integer and the specified Integer,
         * with possible overflow.
         *
         * @param other The Integer to add with this one.
         * @return A new Integer whose value is the sum of this Integer and the
         * specified Integer.
         */
        template <bool other_sliced>
        Integer<bits, false, Placer, p> operator +(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Integer<bits, false, Placer, p>(OpCode::IntAdd, *this, other);
        }

        /**
         * @brief Computes the sum of this Integer and the specified Integer,
         * without overflow; the output carry bit is preserved in the result's
         * most significant bit.
         *
         * The result Integer has one more bit than the operands. Its most
         * significant bit contains the output carry bit for the addition,
         * and the remaining bits contain the result of operator+(). Thus,
         * the result can be viewed as the overflow-free sum, in either
         * unsigned or two's complement representation, of the two operands,
         * extended by one bit to avoid overflow.
         *
         * @param other The Integer to add with this one.
         * @return A new Integer whose value is the overflow-free sum of this
         * Integer and the specified Integer.
         */
        template <bool other_sliced>
        Integer<bits + 1, false, Placer, p> add_with_carry(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Integer<bits + 1, false, Placer, p>(OpCode::IntAddWithCarry, *this, other);
        }

        /**
         * @brief Computes the result of (*this) + Integer<...>(1).
         *
         * While this can be also computed using (*this) + Integer<...>(1),
         * this is often more efficient. In expressing the operation as a
         * binary circuit for secure computation, for example, one can optimize
         * the subcircuit for this operation based on the fact that 1 is a
         * public constant, rather than a secret value.
         *
         * @sa decrement()
         *
         * @return An Integer whose value is one more than this Integer,
         * with possible overflow.
         */
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

        /**
         * @brief Computes the difference of this Integer and the specified
         * Integer, with possible overflow.
         *
         * The result Integer has one more bit than the operands. Its most
         * significant bit contains the output carry bit for the addition,
         * and the remaining bits contain the result of operator+(). Thus,
         * the result can be viewed as the overflow-free sum, in either
         * unsigned or two's complement representation, of the two operands,
         * extended by one bit to avoid overflow.
         *
         * @param other The Integer to subtract from this one.
         * @return A new Integer whose value is the difference of this Integer
         * and the specified Integer.
         */
        template <bool other_sliced>
        Integer<bits, false, Placer, p> operator -(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Integer<bits, false, Placer, p>(OpCode::IntSub, *this, other);
        }

        /**
         * @brief Computes the result of (*this) - Integer<...>(1).
         *
         * While this can be also computed using (*this) - Integer<...>(1),
         * this is often more efficient. In expressing the operation as a
         * binary circuit for secure computation, for example, one can optimize
         * the subcircuit for this operation based on the fact that 1 is a
         * public constant, rather than a secret value.
         *
         * @sa increment()
         *
         * @return An Integer whose value is one less than this Integer,
         * with possible overflow.
         */
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

        /**
         * @brief Computes the product of this Integer and the specified
         * Integer, interpreting both operands as unsigned.
         *
         * The result Integer is wide enough to hold the entire product, so
         * there is no possibility of overflow.
         *
         * Multiplication is significantly more expensive than addition or
         * subtraction, so one should try to use as small a width as possible
         * when performing multiplication.
         *
         * @param other The Integer to multiply with this one.
         * @return A new Integer whose value is the product of this Integer and
         * the specified Integer.
         */
        template <bool other_sliced>
        Integer<2 * bits, false, Placer, p> operator *(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Integer<2 * bits, false, Placer, p>(OpCode::IntMultiply, *this, other);
        }

        /**
         * @brief Outputs a bit that is 1 if this Integer is less than the
         * specified Integer, and 0 otherwise.
         *
         * @param other The Integer to compare with this one.
         * @return A bit indicating if this Integer's value is less than the
         * specified Integer's value.
         */
        template <bool other_sliced>
        Bit<false, Placer, p> operator <(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Bit<false, Placer, p>(OpCode::IntLess, *this, other);
        }

        /**
         * @brief Outputs a bit that is 1 if this Integer is greater than the
         * specified Integer, and 0 otherwise.
         *
         * @param other The Integer to compare with this one.
         * @return A bit indicating if this Integer's value is greater than the
         * specified Integer's value.
         */
        template <bool other_sliced>
        Bit<false, Placer, p> operator >(const Integer<bits, other_sliced, Placer, p>& other) const {
            return other < *this;
        }

        /**
         * @brief Outputs a bit that is 1 if this Integer is less or equal to
         * than the specified Integer, and 0 otherwise.
         *
         * @param other The Integer to compare with this one.
         * @return A bit indicating if this Integer's value is less or equal to
         * the specified Integer's value.
         */
        template <bool other_sliced>
        Bit<false, Placer, p> operator <=(const Integer<bits, other_sliced, Placer, p>& other) const {
            return ~(other < *this);
        }

        /**
         * @brief Outputs a bit that is 1 if this Integer is greater than or
         * equal to the specified Integer, and 0 otherwise.
         *
         * @param other The Integer to compare with this one.
         * @return A bit indicating if this Integer's value is greater than or
         * equal to the specified Integer's value.
         */
        template <bool other_sliced>
        Bit<false, Placer, p> operator >=(const Integer<bits, other_sliced, Placer, p>& other) const {
            return ~((*this) < other);
        }

        /**
         * @brief Outputs a bit that is 1 if this Integer is equal to the
         * specified Integer, and 0 otherwise.
         *
         * @param other The Integer to compare with this one.
         * @return A bit indicating if this Integer's value is equal to the
         * specified Integer's value.
         */
        template <bool other_sliced>
        Bit<false, Placer, p> operator ==(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Bit<false, Placer, p>(OpCode::Equal, *this, other);
        }

        /**
         * @brief Outputs a bit that is 1 if this Integer is equal to 0, and 0
         * otherwise.
         *
         * @param other The Integer to compare with this one.
         * @return A bit indicating if this Integer's value is 0.
         */
        Bit<false, Placer, p> operator !() const {
            return Bit<false, Placer, p>(OpCode::IsZero, *this);
        }

        /**
         * @brief Outputs a bit that is 1 if this Integer is nonzero, and 0
         * otherwise.
         *
         * @param other The Integer to compare with this one.
         * @return A bit indicating if this Integer's value is nonzero.
         */
        Bit<false, Placer, p> nonzero() const {
            return Bit<false, Placer, p>(OpCode::NonZero, *this);
        }

        /**
         * @brief Computes the bitwise negation of this Integer.
         *
         * @return A new Integer whose value is the bitwise negation of this
         * one.
         */
        Integer<bits, false, Placer, p> operator ~() const {
            return Integer<bits, false, Placer, p>(OpCode::BitNOT, *this);
        }

        /**
         * @brief Computes the bitwise AND of this Integer and the specified
         * Integer.
         *
         * @param other The Integer whose bits to AND with this one's.
         * @return A new Integer whose value is the bitwise AND of this
         * Integer and the specified Integer.
         */
        template <bool other_sliced>
        Integer<bits, false, Placer, p> operator &(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Integer<bits, false, Placer, p>(OpCode::BitAND, *this, other);
        }

        /**
         * @brief Computes the bitwise OR of this Integer and the specified
         * Integer.
         *
         * @param other The Integer whose bits to OR with this one's.
         * @return A new Integer whose value is the bitwise OR of this
         * Integer and the specified Integer.
         */
        template <bool other_sliced>
        Integer<bits, false, Placer, p> operator |(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Integer<bits, false, Placer, p>(OpCode::BitOR, *this, other);
        }

        /**
         * @brief Computes the bitwise XOR of this Integer and the specified
         * Integer.
         *
         * @param other The Integer whose bits to XOR with this one's.
         * @return A new Integer whose value is the bitwise XOR of this
         * Integer and the specified Integer.
         */
        template <bool other_sliced>
        Integer<bits, false, Placer, p> operator ^(const Integer<bits, other_sliced, Placer, p>& other) const {
            return Integer<bits, false, Placer, p>(OpCode::BitXOR, *this, other);
        }

        /**
         * @brief Provides a slice of this Integer, which refers to the
         * same underlying memory as this Integer.
         *
         * The sliced Integer refers to memory owned by this Integer, so the
         * sliced Integer should not used after this Integer's memory is
         * deallocated (which happens when it goes out of scope, via a call
         * to recycle(), or due to move assignment.
         *
         * @tparam length The length, in bits, of the sliced Integer.
         * @param The first bit of this Integer (0 is the least significant
         * bit) to include in the slice.
         * @return A slice of this Integer with the requested parameters.
         */
        template <BitWidth length>
        Integer<length, true, Placer, p> slice(BitWidth start) const {
            assert(start + length <= bits);
            assert(this->valid());
            return Integer<length, true, Placer, p>(this->v, start);
        }

        /**
         * @brief Provides a length-1 slice of this Integer, referring to the
         * same underlying memory as a particular bit of this Integer.
         *
         * @param i The index of the bit of this Integer to slice (0 is the
         * least significant bit).
         * @return A slice of this Integer of length 1, including only the
         * requested bit.
         */
        Bit<true, Placer, p> operator [](BitWidth i) const {
            return this->slice<1>(i);
        }

        /**
         * @brief Provides the width, in bits, of this Integer type.
         *
         * @return The width, in bits, of this Integer type.
         */
        static constexpr BitWidth width() {
            return bits;
        }

        /**
         * @brief Multiplexor operation. It provides either one argument or the
         * other, depending on the value of the provided selector bit.
         *
         * @param selector Determines which of the two other arguments this
         * function returns.
         * @param arg0 The Integer whose value to return if selector is 1.
         * @param arg1 The Integer whose value to return if selector is 0.
         * @return A new Integer with the value of either @p arg0 or @p arg1,
         * depending on the value of @p selector.
         */
        template <bool selector_sliced, bool arg0_sliced, bool arg1_sliced>
        static Integer<bits, false, Placer, p> select(const Bit<selector_sliced, Placer, p>& selector, const Integer<bits, arg0_sliced, Placer, p>& arg0, const Integer<bits, arg1_sliced, Placer, p>& arg1) {
            return Integer<bits, false, Placer, p>(OpCode::ValueSelect, arg0, arg1, selector);
        }

        /**
         * @brief Swaps the values of @p arg0 and @p arg1 if @p predicate is 1.
         *
         * This is implemented by composing existing operator and assigning to
         * the provided Integers. So the allocated memory for @p arg0 and
         * @p arg1 may change, invalidated any prior slices of those Integers.
         *
         * @param predicate Determines whether the values of the two other
         * arguments are swapped.
         * @param arg0 A reference to the Integer whose value may be swapped
         * with that of @p arg1.
         * @param arg1 A reference to the Integer whose value may be swapped
         * with that of @p arg0.
         */
        template <bool predicate_sliced>
        static void swap_if(const Bit<predicate_sliced, Placer, p>& predicate, Integer<bits, false, Placer, p>& arg0, Integer<bits, false, Placer, p>& arg1) {
            assert(arg0.valid() && arg1.valid());
            Integer<bits, false, Placer, p> mask = Integer<bits, false, Placer, p>::select(predicate, arg0, arg1) ^ arg1;
            arg0 = arg0 ^ mask;
            arg1 = arg1 ^ mask;
        }

        /**
         * @brief Swaps the values of @p arg0 and @p arg1, if necessary, so
         * that arg0's value is less than or equal to arg1's value after the
         * operation.
         *
         * This is implemented using swap_if(), so the allocated memory for
         * @p arg0 and @p arg1 may change, invalidated any prior slices of
         * those Integers.
         *
         * @param arg0 A reference to the first Integer whose value to compare
         * and potentially swap.
         * @param arg1 A reference to the second Integer whose value to compare
         * and potentially swap.
         */
        static void comparator(Integer<bits, false, Placer, p>& arg0, Integer<bits, false, Placer, p>& arg1) {
            Integer<bits, false, Placer, p>::swap_if(arg0 > arg1, arg0, arg1);
        }

        /**
         * @brief Checks if this Integer is valid (i.e., it has backing memory
         * allocated and its value is safe to use).
         *
         * An invalid Integer can be made valid via one of the following
         * operations: move assignment with a valid Integer, mutate() with a
         * valid Integer, mutate_to_constant(), mark_input(), or
         * post_receive().
         *
         * @return True if this Integer's underlying memory is allocated,
         * otherwise false.
         */
        bool valid() const {
            return this->v != invalid_vaddr;
        }

        /**
         * @brief Returns this Integer to the invalid state. If it is not a
         * slice (i.e., it owns its underlying memory), its memory is first
         * deallocated.
         */
        void recycle() {
            if constexpr (sliced) {
                this->v = invalid_vaddr;
            } else if (this->v != invalid_vaddr) {
                (*p)->recycle(this->v, bits);
                this->v = invalid_vaddr;
            }
        }


    private:
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

        /**
         * @brief Address of the underlying data.
         */
        VirtAddr v;
    };
}

#endif
