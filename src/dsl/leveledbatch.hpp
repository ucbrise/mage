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
 * @file dsl/leveledbatch.hpp
 * @brief Leveled Batch DSL for writing programs for MAGE.
 */

#ifndef MAGE_DSL_LEVELEDBATCH_HPP_
#define MAGE_DSL_LEVELEDBATCH_HPP_

#include <cassert>
#include <cstdint>
#include "instruction.hpp"
#include "memprog/program.hpp"
#include "addr.hpp"
#include "opcode.hpp"

namespace mage::dsl {
    using memprog::Program;

    template <std::int32_t level, bool normalized, typename Placer, Program<Placer>** p>
    class LeveledBatch;

    /**
     * @brief Similar to LeveledBatch, but represents plaintext data instead of
     * encrypted data.
     *
     * @sa LeveledBatch
     */
    template <std::int32_t level, typename Placer, Program<Placer>** p>
    class LeveledPlaintextBatch {
        static_assert(level >= 0 && level <= 2);

        template <std::int32_t other_level, bool other_normalized, typename OtherPlacer, Program<OtherPlacer>** other_p>
        friend class LeveledBatch;

    public:
        /**
         * @brief Creates an invalid LeveledPlaintextBatch, with no underlying
         * memory.
         *
         * Before using the created LeveledPlaintextBatch, one should perform
         * an operation that allocates memory for this LeveledPlaintextBatch,
         * like move assignment.
         */
        LeveledPlaintextBatch() : v(invalid_vaddr) {
        }

        /**
         * @brief Creates a LeveledPlaintextBatch, allocates fresh memory for
         * it, and initializes all slots in the batch to the provided constant.
         *
         * @param public_constant The value to which to initialize all slots
         * in this LeveledPlaintextBatch.
         */
        LeveledPlaintextBatch(double public_constant) {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::Encode;
            instr.header.width = level;
            instr.header.flags = 0;
            instr.constant.constant = *reinterpret_cast<std::uint64_t*>(&public_constant);
            this->v = (*p)->commit_instruction(this->get_size());
        }

        /**
         * @brief LeveledPlaintextBatch is not copy-constructible.
         */
        LeveledPlaintextBatch(LeveledPlaintextBatch<level, Placer, p>& other) = delete;

        /**
         * @brief Move-constructs a LeveledPlaintextBatch, setting its value to
         * that of* the specified Integer.
         *
         * Instead of copying the value from one LeveledPlaintextBatch to
         * another, ownership of the underlying memory is transferred from the
         * provided LeveledPlaintextBatch to this one. Thus, this operation has
         * zero runtime cost.
         *
         * After a LeveledPlaintextBatch is used as an argument to the move
         * constructor, it is invalid, as if it were constructed with the
         * default constructor.
         *
         * @param other The LeveledPlaintextBatch to whose value this
         * LeveledPlaintextBatch should be set.
         */
        LeveledPlaintextBatch(LeveledPlaintextBatch<level, Placer, p>&& other) : v(other.v) {
            other.v = invalid_vaddr;
        }

        /**
         * @brief Returns this LeveledPlaintextBatch to an invalid state, if it
         * is valid.
         */
        ~LeveledPlaintextBatch() {
            this->recycle();
        }

        /**
         * @brief LeveledPlaintextBatch is not copy-assignable.
         */
        LeveledPlaintextBatch<level, Placer, p>& operator =(LeveledPlaintextBatch<level, Placer, p>& other) = delete;

        /**
         * @brief Move-assigns a LeveledPlaintextBatch, setting its value to
         * that of the specified LeveledPlaintextBatch.
         *
         * All the details of the move constructor also apply here.
         *
         * @param other The LeveledPlaintextBatch to whose value this
         * LeveledPlaintextBatch should be set.
         */
        LeveledPlaintextBatch<level, Placer, p>& operator =(LeveledPlaintextBatch<level, Placer, p>&& other) {
            this->recycle();
            this->v = other.v;
            other.v = invalid_vaddr;
            return *this;
        }

    private:
        void recycle() {
            if (this->v != invalid_vaddr) {
                (*p)->recycle(this->v, this->get_size());
                this->v = invalid_vaddr;
            }
        }

        static constexpr std::size_t get_size() {
            return (*p)->get_physical_width(level, memprog::PlaceableType::Plaintext);
        }

        /**
         * @brief Pointer to the underlying data in the MAGE-virtual address
         * space.
         */
        VirtAddr v;
    };

    /**
     * @brief A LeveledBatch is a batch of real numbers, supporting addition
     * and multiplication in a SIMD manner. The batch has an associated level,
     * which is reduced when normalizing the result of a multiplication.
     * Two batches can only be multiplied if their level is nonzero and if
     * both factors are normalized. I
     *
     * This abstraction is designed for Leveled FHE schemes, like CKKS.
     * The emitted instructions are designed to work with the AND-XOR engine.
     *
     * Like instances of the Integer class, instances of the LeveledBatch class
     * can be understood as storing a pointer to memory in a MAGE-virtual
     * address space. Unlike an Integer, a LeveledBatch cannot be "sliced"; it
     * was designed this way under the assumption that the underlying secure
     * computation protocol is unlikely to support computation on units of
     * data smaller than a single batch; a LeveledBatch corresponds to a single
     * ciphertext. Thus, every LeveledBatch has ownership of its underlying
     * memory and deallocates it when the LeveledBatch goes out of scope.
     *
     * In some cases, a LeveledBatch's MAGE-virtual memory may not be
     * allocated. For example, this will happen if the default constructor is
     * used to create a LeveledBatch. It will also happen if it is used to
     * move-construct or move-assign to another LeveledBatch. A LeveledBatch in
     * such a state is said to be invalid.
     *
     * When an operation is performed using Integer objects, this class takes
     * the following steps: (1) it uses @p p to allocate space in the
     * MAGE-virtual address space, (2) it initializes and returns a new
     * LeveledBatch object representing the newly allocated space in the
     * MAGE-virtual address space, and (3) it emits an instruction using @p p
     * to perform the operation, reading its arguments from thespaces in the
     * MAGE-virtual address space corresponding to the LeveledBatch arguments
     * and storing the result in the newly allocated space in the MAGE-virtual
     * address space.
     *
     * The documentation below generally describes the effect that the
     * functions have in the program. In reality, when the functions below are
     * executed, they emit instructions that perform the described actions.
     *
     * In contrast to LeveledPlaintextBatch, which represents a plaintext batch
     * of items, LeveledBatch represents a ciphertext batch of items.
     *
     * @sa LeveledPlaintextBatch
     *
     * @tparam level The remaining multiplicative depth supported by this
     * LeveledBatch. It decreases by 1 after each normalization; once the level
     * is 0, multiplication is not supported. Theoretically, a procedure called
     * bootstrapping can increase the level, but it is very slow and is often
     * not supported.
     * @tparam normalized Indicates if the LeveledBatch is normalized. After a
     * multiplication, the product is not normalized. It must be normalized
     * before being used in subsequent multiplications. Normalizing a
     * LeveledBatch also decreases its memory footprint.
     * @tparam Placer Type of the placement algorithm used to allocate and
     * deallocate memory in the MAGE-virtual address space.
     * @tparam p Double pointer to the program object with access to MAGE's
     * placement module and to the intermediate bytecode being written.
     */
    template <std::int32_t level, bool normalized, typename Placer, Program<Placer>** p>
    class LeveledBatch {
        template <std::int32_t other_level, bool other_normalized, typename OtherPlacer, Program<OtherPlacer>** other_p>
        friend class LeveledBatch;

        static_assert(level >= 0 && level <= 2);

    public:
        /**
         * @brief Creates an invalid LeveledBatch, with no underlying memory.
         * Before using this LeveledBatch, one should call mark_input() or
         * another operation that allocates memory for this LeveledBatch.
         */
        LeveledBatch() : v(invalid_vaddr) {
        }

        /**
         * @brief Creates a LeveledBatch, allocates fresh memory for it, and
         * and initializes all slots in the batch to the provided constant.
         *
         * @param public_constant The value to which to initialize all slots
         * in this LeveledBatch.
         */
        LeveledBatch(double public_constant) {
            static_assert(normalized);

            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::PublicConstant;
            instr.header.width = level;
            instr.header.flags = 0;
            instr.constant.constant = *reinterpret_cast<std::uint64_t*>(&public_constant);
            this->v = (*p)->commit_instruction(this->get_size());
        }

        /**
         * @brief LeveledBatch is not copy-constructible.
         */
        LeveledBatch(const LeveledBatch<level, normalized, Placer, p>& other) = delete;

        /**
         * @brief Move-constructs a LeveledBatch, setting its value to that of
         * the specified Integer.
         *
         * Instead of copying the value from one LeveledBatch to another,
         * ownership of the underlying memory is transferred from the provided
         * LeveledBatch to this one. Thus, this operation has zero runtime
         * cost.
         *
         * After a LeveledBatch is used as an argument to the move constructor,
         * it is invalid, as if it were constructed with the default
         * constructor.
         *
         * @param other The LeveledBatch to whose value this LeveledBatch
         * should be set.
         */
        LeveledBatch(LeveledBatch<level, normalized, Placer, p>&& other) : v(other.v) {
            other.v = invalid_vaddr;
        }

        /**
         * @brief Returns this LeveledBatch to an invalid state, if it is
         * valid.
         */
        ~LeveledBatch() {
            this->recycle();
        }

        /**
         * @brief Overwrites the value of this LeveledBatch with ciphertext
         * data read from the program's input.
         *
         * @param party The party whose input to read (only applicable if
         * the underlying protocol supports multiple parties).
         */
        void mark_input() {
            this->recycle();

            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::Input;
            instr.header.width = level;
            instr.header.flags = normalized ? 0x0 : FlagNotNormalized;
            this->v = (*p)->commit_instruction(this->get_size());
        }

        /**
         * @brief Writes the value of this LeveledBatch as a ciphertext to the
         * program's output.
         */
        void mark_output() {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::Output;
            instr.header.width = level;
            instr.header.flags = normalized ? 0x0 : FlagNotNormalized;
            instr.header.output = this->v;
            (*p)->commit_instruction(0);
        }

        /**
         * @brief LeveledBatch is not copy-assignable.
         */
        LeveledBatch<level, normalized, Placer, p>& operator =(const LeveledBatch<level, normalized, Placer, p>& other) = delete;

        /**
         * @brief Move-assigns a LeveledBatch, setting its value to that of the
         * specified LeveledBatch.
         *
         * All the details of the move constructor also apply here.
         *
         * @param other The LeveledBatch to whose value this LeveledBatch
         * should be set.
         */
        LeveledBatch<level, normalized, Placer, p>& operator =(LeveledBatch<level, normalized, Placer, p>&& other) {
            this->recycle();
            this->v = other.v;
            other.v = invalid_vaddr;
            return *this;
        }

        /**
         * @brief Copies the value of the specified LeveledBatch into this
         * LeveledBatch, allocating fresh memory for this Integer, if necessary.
         *
         * Unlike the move constructor or move assignment, ownership of the
         * underlying memory is not transferred to this Integer. Instead, the
         * value of the specified Integer is copied to this one, which has a
         * nonzero runtime cost. If this is an invalid Integer (no
         * underlying memory is allocated), a fresh block of memory is
         * allocated for this Integer, this Integer is initialized to use that
         * memory, and the specified Integer's value is copied to that memory.
         *
         * Logically, the behavior here is similar to what one may expect to
         * see in a copy constructor or copy assignment operator. The copy
         * constructor and copy assignment operators are deleted to avoid
         * "hiding" copy operations from the programmer. Forcing the programmer
         * to call mutate() makes the copies more visible in the code.
         *
         * @param other The LeveledBatch object whose value to copy into this
         * one.
         */
        void mutate(LeveledBatch<level, normalized, Placer, p>& other) {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::Copy;
            instr.header.width = level;
            instr.header.flags = 0;
            instr.one_arg.input1 = other.v;
            if (this->valid()) {
                instr.header.output = this->v;
                (*p)->commit_instruction(0);
            } else {
                this->v = (*p)->commit_instruction(this->get_size());
            }
        }

        /**
         * @brief Sends the contents of this LeveledBatch to the specified
         * worker.
         *
         * The data is enqueued into a buffer; it may not be sent immediately.
         * To force the data to be sent, use finish_send().
         *
         * @param to The ID of the worker to which the data should be sent.
         */
        void buffer_send(WorkerID to) const {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::NetworkBufferSend;
            instr.header.width = level;
            instr.header.flags = normalized ? 0x0 : FlagNotNormalized;;
            instr.header.output = this->v;
            instr.constant.constant = to;
            (*p)->commit_instruction(0);
        }

        /**
         * @brief Overwrites the value of this LeveledBatch with data received
         * from the specified worker, allocating the underlying memory for this
         * LeveledBatch if it was previously invalid.
         *
         * Once this function returns, the receive has been initiated but has
         * not necessarily completed. To wait until the desired data has been
         * received from the worker, use finish_receive().
         *
         * @param from The ID of the worker from which data should be received.
         */
        void post_receive(WorkerID from) {
            this->recycle();

            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::NetworkPostReceive;
            instr.header.width = level;
            instr.header.flags = normalized ? 0x0 : FlagNotNormalized;;
            instr.constant.constant = from;
            this->v = (*p)->commit_instruction(this->get_size());
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
         * @brief Computes the element-wise sum of this LeveledBatch and the
         * specified LeveledBatch.
         *
         * @param other The LeveledBatch to add with this one.
         * @return A new LeveledBatch containing the element-wise sum of this
         * LeveledBatch and the specified LeveledBatch.
         */
        LeveledBatch<level, normalized, Placer, p> operator +(const LeveledBatch<level, normalized, Placer, p>& other) {
            static_assert(level >= 0);
            return LeveledBatch<level, normalized, Placer, p>(OpCode::IntAdd, *this, other);
        }

        /**
         * @brief Computes the element-wise sum of this LeveledBatch and the
         * specified LeveledBatch.
         *
         * @param other The LeveledBatch to subtract from this one.
         * @return A new LeveledBatch containing the element-wise sum of this
         * LeveledBatch and the specified LeveledBatch.
         */
        LeveledBatch<level, normalized, Placer, p> operator -(const LeveledBatch<level, normalized, Placer, p>& other) {
            static_assert(level >= 0);
            return LeveledBatch<level, normalized, Placer, p>(OpCode::IntSub, *this, other);
        }

        /**
         * @brief Computes the element-wise sum of this LeveledBatch and the
         * specified batch of plaintexts.
         *
         * @param other The batch of plaintexts to add with this one.
         * @return A new LeveledBatch containing the element-wise sum of this
         * LeveledBatch and the specified batch of plaintexts.
         */
        LeveledBatch<level, normalized, Placer, p> operator +(const LeveledPlaintextBatch<level, Placer, p>& other) {
            static_assert(level >= 0);
            return LeveledBatch<level, normalized, Placer, p>(OpCode::AddPlaintext, *this, other);
        }

        /**
         * @brief Computes the element-wise product of this LeveledBatch and
         * the specified LeveledBatch, and normalizes the result so that it can
         * be used right away in subsequent multiplications.
         *
         * @param other The LeveledBatch to multiply with this one.
         * @return A new LeveledBatch containing the element-wise product of
         * this LeveledBatch and the specified LeveledBatch.
         */
        LeveledBatch<level - 1, true, Placer, p> operator *(const LeveledBatch<level, true, Placer, p>& other) {
            static_assert(level > 0);
            static_assert(normalized);
            return LeveledBatch<level - 1, normalized, Placer, p>(OpCode::IntMultiply, *this, other);
        }

        /**
         * @brief Computes the element-wise product of this LeveledBatch and
         * the specified LeveledBatch, without normalizing the result.
         *
         * This allows one to delay normalization when it is more efficient to
         * do so. For example, if one wants to compute an expression of the
         * form ab + cd, one can compute ab and cd without normalization, add
         * the non-normalized products and perform a single normalization of
         * the overall result. This requires fewer normalizations, and is
         * therefore more efficient, than normalizing ab and cd before
         * computing the sum.
         *
         * @param other The LeveledBatch to multiply with this one.
         * @return A new LeveledBatch containing the element-wise product of
         * this LeveledBatch and the specified LeveledBatch.
         */
        LeveledBatch<level, false, Placer, p> multiply_without_normalizing(const LeveledBatch<level, true, Placer, p>& other) {
            static_assert(level > 0);
            static_assert(normalized);
            return LeveledBatch<level, false, Placer, p>(OpCode::MultiplyRaw, *this, other);
        }

        /**
         * @brief Computes the element-wise product of this LeveledBatch and
         * the specified batch of plaintexts.
         *
         * @param other The batch of plaintexts to multiply with this one.
         * @return A new LeveledBatch containing the element-wise product of
         * this LeveledBatch and the specified batch of plaintexts.
         */
        LeveledBatch<level - 1, true, Placer, p> operator *(const LeveledPlaintextBatch<level, Placer, p>& plaintext) {
            static_assert(level > 0);
            static_assert(normalized);
            return LeveledBatch<level - 1, normalized, Placer, p>(OpCode::MultiplyPlaintext, *this, plaintext);
        }

        /**
         * @brief Converts a non-normalized LeveledBatch into a normalized one,
         * decreasing its level by 1.
         *
         * @return A normalized LeveledBatch whose level is one smaller than
         * this one.
         */
        LeveledBatch<level - 1, true, Placer, p> renormalize() {
            static_assert(level > 0);
            static_assert(!normalized);
            return LeveledBatch<level - 1, true, Placer, p>(OpCode::Renormalize, *this);
        }

        /**
         * @brief Decreases the level of a LeveledBatch without performing a
         * semantic computation with it.
         *
         * This can be used to skip levels, as computing on LeveledBatch
         * instances with lower levels is generally more efficient than
         * computing on LeveledBatch instances with higher levels. It may also
         * be needed to ensure that both arguments to an addition or
         * multiplication operation have the same level.
         *
         * @return A normalized LeveledBatch whose level is one smaller than
         * this one.
         */
        LeveledBatch<level - 1, true, Placer, p> switch_level() {
            static_assert(level > 0);
            static_assert(normalized);
            return LeveledBatch<level - 1, normalized, Placer, p>(OpCode::SwitchLevel, *this);
        }

        /**
         * @brief Checks if this LeveledBatch is valid (i.e., it has backing
         * memory allocated and its value is safe to use).
         *
         * An invalid LeveledBatch can be made valid via one of the following
         * operations: move assignment with a valid LeveledBatch, mutate() with
         * valid LeveledBatch, mark_input(), or post_receive().
         *
         * @return True if this LeveledBatch's underlying memory is allocated,
         * otherwise false.
         */
        bool valid() const {
            return this->v != invalid_vaddr;
        }

        /**
         * @brief Returns this LeveledBatch to the invalid state. If it is
         * valid, its memory is first deallocated.
         */
        void recycle() {
            if (this->v != invalid_vaddr) {
                (*p)->recycle(this->v, this->get_size());
                this->v = invalid_vaddr;
            }
        }

        /**
         * @brief Provides the size, in bytes, of this LeveledBatch type.
         *
         * It may depend on the ciphertext sizes of the target secure
         * computation protocol.
         *
         * @return The size, in bytes, of this LeveledBatch type.
         */
        static std::size_t get_size() {
            // return protocols::ckks::ckks_ciphertext_size(level, normalized);
            constexpr memprog::PlaceableType t = normalized ? memprog::PlaceableType::Ciphertext : memprog::PlaceableType::DenormalizedCiphertext;
            return (*p)->get_physical_width(level, t);
        }

    private:
        template <std::int32_t arg0_level, bool arg0_normalized>
        LeveledBatch(OpCode operation, const LeveledBatch<arg0_level, arg0_normalized, Placer, p>& arg0) {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = operation;
            instr.header.width = level;
            instr.header.flags = normalized ? 0x0 : FlagNotNormalized;
            instr.one_arg.input1 = arg0.v;
            this->v = (*p)->commit_instruction(this->get_size());
        }

        template <std::int32_t arg0_level, bool arg0_normalized, typename Arg1>
        LeveledBatch(OpCode operation, const LeveledBatch<arg0_level, arg0_normalized, Placer, p>& arg0, const Arg1& arg1) {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = operation;
            instr.header.width = level;
            instr.header.flags = normalized ? 0x0 : FlagNotNormalized;
            instr.two_args.input1 = arg0.v;
            instr.two_args.input2 = arg1.v;
            this->v = (*p)->commit_instruction(this->get_size());
        }

        /**
         * @brief Pointer to the underlying data in the MAGE-virtual address
         * space.
         */
        VirtAddr v;
    };
}

#endif
