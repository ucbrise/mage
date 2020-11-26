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

#ifndef MAGE_DSL_LEVELEDBATCH_HPP_
#define MAGE_DSL_LEVELEDBATCH_HPP_

#include <cassert>
#include <cstdint>
#include "instruction.hpp"
#include "memprog/program.hpp"
#include "protocols/ckks_constants.hpp"
#include "addr.hpp"
#include "opcode.hpp"

namespace mage::dsl {
    using memprog::Program;

    template <std::int32_t level, typename Placer, Program<Placer>** p>
    class LeveledBatch {
        template <std::int32_t other_level, typename OtherPlacer, Program<OtherPlacer>** other_p>
        friend class LeveledBatch;

        static_assert(level >= -1 && level <= 2);

    public:
        LeveledBatch() : v(invalid_vaddr) {
        }

        LeveledBatch(float public_constant) {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::PublicConstant;
            instr.header.width = level;
            instr.header.flags = 0;
            instr.constant.constant = *reinterpret_cast<std::uint32_t*>(&public_constant);
            this->v = (*p)->commit_instruction(this->get_size());
        }

        LeveledBatch(const LeveledBatch<level, Placer, p>& other) = delete;

        LeveledBatch(LeveledBatch<level, Placer, p>&& other) : v(other.v) {
            other.v = invalid_vaddr;
        }

        ~LeveledBatch() {
            this->recycle();
        }

        void mark_input() {
            this->recycle();

            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::Input;
            instr.header.width = level;
            instr.header.flags = 0;
            this->v = (*p)->commit_instruction(this->get_size());
        }

        void mark_output() {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::Output;
            instr.header.width = level;
            instr.header.flags = 0;
            instr.header.output = this->v;
            (*p)->commit_instruction(0);
        }

        LeveledBatch<level, Placer, p>& operator =(const LeveledBatch<level, Placer, p>& other) = delete;

        LeveledBatch<level, Placer, p>& operator =(LeveledBatch<level, Placer, p>&& other) {
            this->recycle();
            this->v = other.v;
            other.v = invalid_vaddr;
            return *this;
        }

        void buffer_send(WorkerID to) const {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::NetworkBufferSend;
            instr.header.width = level;
            instr.header.flags = 0;
            instr.header.output = this->v;
            instr.constant.constant = to;
            (*p)->commit_instruction(0);
        }

        void post_receive(WorkerID from) {
            this->recycle();

            Instruction& instr = (*p)->instruction();
            instr.header.operation = OpCode::NetworkPostReceive;
            instr.header.width = level;
            instr.header.flags = 0;
            instr.constant.constant = from;
            this->v = (*p)->commit_instruction(this->get_size());
        }

        static void finish_send(WorkerID to) {
            (*p)->finish_send(to);
        }

        static void finish_receive(WorkerID from) {
            (*p)->finish_receive(from);
        }

        LeveledBatch<level, Placer, p> operator +(const LeveledBatch<level, Placer, p>& other) {
            static_assert(level > -1);
            return LeveledBatch<level, Placer, p>(OpCode::IntAdd, *this, other);
        }

        LeveledBatch<level - 1, Placer, p> operator *(const LeveledBatch<level, Placer, p>& other) {
            static_assert(level > 0);
            return LeveledBatch<level - 1, Placer, p>(OpCode::IntMultiply, *this, other);
        }

        LeveledBatch<level - 1, Placer, p> operator *(const LeveledBatch<-1, Placer, p>& plaintext) {
            static_assert(level > 0);
            return LeveledBatch<level - 1, Placer, p>(OpCode::MultiplyPlaintext, *this, plaintext);
        }

        LeveledBatch<level - 1, Placer, p> switch_level() {
            static_assert(level > 0);
            return LeveledBatch<level - 1, Placer, p>(OpCode::SwitchLevel, *this);
        }

        bool valid() const {
            return this->v != invalid_vaddr;
        }

        void recycle() {
            if (this->v != invalid_vaddr) {
                (*p)->recycle(this->v, this->get_size());
                this->v = invalid_vaddr;
            }
        }

        static constexpr std::size_t get_size() {
            return schemes::ckks_ciphertext_size(level);
        }

    private:
        template <std::int32_t arg0_level>
        LeveledBatch(OpCode operation, const LeveledBatch<arg0_level, Placer, p>& arg0) {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = operation;
            instr.header.width = level;
            instr.header.flags = 0;
            instr.one_arg.input1 = arg0.v;
            this->v = (*p)->commit_instruction(this->get_size());
        }

        template <std::int32_t arg0_level, std::int32_t arg1_level>
        LeveledBatch(OpCode operation, const LeveledBatch<arg0_level, Placer, p>& arg0, const LeveledBatch<arg1_level, Placer, p>& arg1) {
            Instruction& instr = (*p)->instruction();
            instr.header.operation = operation;
            instr.header.width = level;
            instr.header.flags = 0;
            instr.two_args.input1 = arg0.v;
            instr.two_args.input2 = arg1.v;
            this->v = (*p)->commit_instruction(this->get_size());
        }

        VirtAddr v;
    };
}

#endif
