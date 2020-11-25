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

#ifndef MAGE_ENGINE_ANDXOR_HPP_
#define MAGE_ENGINE_ANDXOR_HPP_

#include <iostream>
#include <memory>
#include <string>
#include "addr.hpp"
#include "engine/engine.hpp"
#include "programfile.hpp"
#include "util/config.hpp"
#include "util/misc.hpp"
#include "util/stats.hpp"

namespace mage::engine {
    template <typename ProtEngine>
    class ANDXOREngine : private Engine {
    public:
        ANDXOREngine(const std::shared_ptr<ClusterNetwork>& network, const util::ConfigValue& worker, ProtEngine& prot, std::string program)
            : Engine(network), protocol(prot), input(program.c_str()) {
            const ProgramFileHeader& header = this->input.get_header();
            if (worker.get("storage_path") == nullptr) {
                std::cerr << "No storage path is specified for this worker" << std::endl;
                std::abort();
            }
            PageShift wire_page_shift = header.page_shift;
            PageSize wire_page_size = pg_size(header.page_shift);
            PageSize byte_page_size = wire_page_size * sizeof(typename ProtEngine::Wire);
            this->init(worker["storage_path"].as_string(), byte_page_size, header.num_pages, header.num_swap_pages, header.max_concurrent_swaps);
            this->input.enable_stats("READ-INSTR (ns)");
            this->wires = reinterpret_cast<typename ProtEngine::Wire*>(this->get_memory());
        }

        void execute_program() {
            InstructionNumber num_instructions = this->input.get_header().num_instructions;
            for (InstructionNumber i = 0; i != num_instructions; i++) {
                PackedPhysInstruction& phys = this->input.start_instruction();
                std::size_t size = this->execute_instruction(phys);
                this->input.finish_instruction(size);
            }
        }

        void execute_public_constant(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.constant.output];
            BitWidth width = phys.constant.width;
            std::uint64_t constant = phys.constant.constant;

            for (BitWidth i = 0; i != width; i++) {
                if ((constant & 0x1) == 0) {
                    this->protocol.zero(output[i]);
                } else {
                    this->protocol.one(output[i]);
                }
                constant = constant >> 1;
            }
        }

        void execute_copy(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.one_arg.output];
            typename ProtEngine::Wire* input = &this->wires[phys.one_arg.input1];
            BitWidth width = phys.one_arg.width;

            std::copy(input, input + width, output);
        }

        template <bool final_carry>
        void execute_int_add(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.two_args.output];
            typename ProtEngine::Wire* input1 = &this->wires[phys.two_args.input1];
            typename ProtEngine::Wire* input2 = &this->wires[phys.two_args.input2];
            BitWidth width = phys.two_args.width;

            typename ProtEngine::Wire temp1;
            typename ProtEngine::Wire temp2;
            typename ProtEngine::Wire temp3;

            typename ProtEngine::Wire carry;
            this->protocol.zero(carry);
            this->protocol.op_copy(temp1, input1[0]);
            this->protocol.op_copy(temp2, input2[0]);
            this->protocol.op_xor(output[0], temp1, temp2);
            for (BitWidth i = 1; i != width; i++) {
                /* Calculate carry from previous adder. */
                this->protocol.op_and(temp3, temp1, temp2);
                this->protocol.op_xor(carry, carry, temp3);

                this->protocol.op_xor(temp1, input1[i], carry);
                this->protocol.op_xor(temp2, input2[i], carry);
                this->protocol.op_xor(output[i], temp1, input2[i]);
            }
            if constexpr (final_carry) {
                this->protocol.op_and(temp3, temp1, temp2);
                this->protocol.op_xor(output[width], carry, temp3);
            }
        }

        void execute_int_increment(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.one_arg.output];
            typename ProtEngine::Wire* input = &this->wires[phys.one_arg.input1];
            BitWidth width = phys.one_arg.width;

            typename ProtEngine::Wire carry;
            this->protocol.op_not(output[0], input[0]);
            this->protocol.op_copy(carry, input[0]);
            if (width == 1) {
                return;
            }
            for (BitWidth i = 1; i != width - 1; i++) {
                this->protocol.op_xor(output[i], input[i], carry);
                this->protocol.op_and(carry, carry, input[i]);
            }
            this->protocol.op_xor(output[width - 1], input[width - 1], carry);
            // skip computing the final output carry
        }

        void execute_int_sub(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.two_args.output];
            typename ProtEngine::Wire* input1 = &this->wires[phys.two_args.input1];
            typename ProtEngine::Wire* input2 = &this->wires[phys.two_args.input2];
            BitWidth width = phys.two_args.width;

            typename ProtEngine::Wire temp1;
            typename ProtEngine::Wire temp2;
            typename ProtEngine::Wire temp3;

            typename ProtEngine::Wire borrow;
            this->protocol.zero(borrow);
            this->protocol.op_copy(temp1, input1[0]);
            this->protocol.op_copy(temp2, input2[0]);
            this->protocol.op_xor(output[0], temp1, temp2);
            for (BitWidth i = 1; i != width; i++) {
                /* Calculate carry from previous adder. */
                this->protocol.op_and(temp3, temp1, temp2);
                this->protocol.op_xor(borrow, borrow, temp3);

                this->protocol.op_xor(temp1, input1[i], input2[i]);
                this->protocol.op_xor(temp2, input2[i], borrow);
                this->protocol.op_xor(output[i], temp1, borrow);
            }
        }

        void execute_int_decrement(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.one_arg.output];
            typename ProtEngine::Wire* input = &this->wires[phys.one_arg.input1];
            BitWidth width = phys.one_arg.width;

            typename ProtEngine::Wire borrow;
            this->protocol.op_not(borrow, input[0]);
            this->protocol.op_copy(output[0], borrow);
            if (width == 1) {
                return;
            }
            for (BitWidth i = 1; i != width - 1; i++) {
                this->protocol.op_xor(output[i], input[i], borrow);
                this->protocol.op_and(borrow, borrow, output[i]);
            }
            this->protocol.op_xor(output[width - 1], input[width - 1], borrow);
            // skip computing the final output carry
        }

        void execute_int_multiply(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.two_args.output];
            typename ProtEngine::Wire* input1 = &this->wires[phys.two_args.input1];
            typename ProtEngine::Wire* input2 = &this->wires[phys.two_args.input2];
            BitWidth operand_width = phys.two_args.width;
            BitWidth product_width = operand_width << 1;

            if (operand_width == 0) {
                return;
            }

            for (BitWidth j = 0; j != operand_width; j++) {
                this->protocol.op_and(output[j], input1[j], input2[0]);
            }
            this->protocol.zero(output[operand_width]);

            for (BitWidth i = 1; i != operand_width; i++) {
                typename ProtEngine::Wire partial_product[operand_width];
                for (BitWidth j = 0; j != operand_width; j++) {
                    this->protocol.op_and(partial_product[j], input1[j], input2[i]);
                }

                /* Add partial_product to output starting at bit i. */
                typename ProtEngine::Wire temp1;
                typename ProtEngine::Wire temp2;
                typename ProtEngine::Wire temp3;
                typename ProtEngine::Wire carry;
                this->protocol.zero(carry);
                for (BitWidth j = 0; j != operand_width; j++) {
                    this->protocol.op_xor(temp1, output[i + j], carry);
                    this->protocol.op_xor(temp2, partial_product[j], carry);
                    this->protocol.op_xor(output[i + j], temp1, partial_product[j]);
                    this->protocol.op_and(temp3, temp1, temp2);
                    this->protocol.op_xor(carry, carry, temp3);
                }
                this->protocol.op_copy(output[i + operand_width], carry);
            }
        }

        /* Based on https://github.com/samee/obliv-c/blob/obliv-c/src/ext/oblivc/obliv_bits.c */
        void execute_int_less(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.two_args.output];
            typename ProtEngine::Wire* input1 = &this->wires[phys.two_args.input1];
            typename ProtEngine::Wire* input2 = &this->wires[phys.two_args.input2];
            BitWidth width = phys.two_args.width;

            typename ProtEngine::Wire result;

            typename ProtEngine::Wire temp1;
            typename ProtEngine::Wire temp2;
            typename ProtEngine::Wire temp3;

            this->protocol.op_xor(temp1, input1[0], input2[0]);
            this->protocol.op_and(result, temp1, input2[0]);
            for (BitWidth i = 1; i != width; i++) {
                this->protocol.op_xor(temp1, input1[i], input2[i]);
                this->protocol.op_xor(temp2, input2[i], result);
                this->protocol.op_and(temp3, temp1, temp2);
                this->protocol.op_xor(result, result, temp3);
            }

            this->protocol.op_copy(*output, result);
        }

        void execute_equal(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.two_args.output];
            typename ProtEngine::Wire* input1 = &this->wires[phys.two_args.input1];
            typename ProtEngine::Wire* input2 = &this->wires[phys.two_args.input2];
            BitWidth width = phys.two_args.width;

            typename ProtEngine::Wire result;
            this->protocol.op_xnor(result, input1[0], input2[0]);

            typename ProtEngine::Wire temp;
            for (BitWidth i = 1; i != width; i++) {
                this->protocol.op_xnor(temp, input1[i], input2[i]);
                this->protocol.op_and(result, result, temp);
            }
            this->protocol.op_copy(*output, result);
        }

        void execute_is_zero(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.one_arg.output];
            typename ProtEngine::Wire* input = &this->wires[phys.one_arg.input1];
            BitWidth width = phys.one_arg.width;

            typename ProtEngine::Wire result;
            this->protocol.op_copy(result, input[0]);

            typename ProtEngine::Wire temp;
            for (BitWidth i = 0; i != width; i++) {
                this->protocol.op_not(temp, input[i]);
                this->protocol.op_and(result, result, temp);
            }
            this->protocol.op_copy(*output, result);
        }

        void execute_non_zero(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.one_arg.output];
            typename ProtEngine::Wire* input = &this->wires[phys.one_arg.input1];
            BitWidth width = phys.one_arg.width;

            typename ProtEngine::Wire result;
            this->protocol.op_copy(result, input[0]);

            typename ProtEngine::Wire temp;
            for (BitWidth i = 0; i != width; i++) {
                this->protocol.op_not(temp, input[i]);
                this->protocol.op_and(result, result, temp);
            }
            this->protocol.op_not(*output, result);
        }

        void execute_bit_not(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.one_arg.output];
            typename ProtEngine::Wire* input = &this->wires[phys.one_arg.input1];
            BitWidth width = phys.one_arg.width;

            for (BitWidth i = 0; i != width; i++) {
                this->protocol.op_not(output[i], input[i]);
            }
        }

        void execute_bit_and(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.two_args.output];
            typename ProtEngine::Wire* input1 = &this->wires[phys.two_args.input1];
            typename ProtEngine::Wire* input2 = &this->wires[phys.two_args.input2];
            BitWidth width = phys.two_args.width;

            for (BitWidth i = 0; i != width; i++) {
                this->protocol.op_and(output[i], input1[i], input2[i]);
            }
        }

        void execute_bit_or(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.two_args.output];
            typename ProtEngine::Wire* input1 = &this->wires[phys.two_args.input1];
            typename ProtEngine::Wire* input2 = &this->wires[phys.two_args.input2];
            BitWidth width = phys.two_args.width;

            typename ProtEngine::Wire temp1;
            typename ProtEngine::Wire temp2;
            for (BitWidth i = 0; i != width; i++) {
                this->protocol.op_xor(temp1, input1[i], input2[i]);
                this->protocol.op_and(temp2, input1[i], input2[i]);
                this->protocol.op_xor(output[i], temp1, temp2);
            }
        }

        void execute_bit_xor(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.two_args.output];
            typename ProtEngine::Wire* input1 = &this->wires[phys.two_args.input1];
            typename ProtEngine::Wire* input2 = &this->wires[phys.two_args.input2];
            BitWidth width = phys.two_args.width;

            for (BitWidth i = 0; i != width; i++) {
                this->protocol.op_xor(output[i], input1[i], input2[i]);
            }
        }

        void execute_value_select(const PackedPhysInstruction& phys) {
            typename ProtEngine::Wire* output = &this->wires[phys.three_args.output];
            typename ProtEngine::Wire* input1 = &this->wires[phys.three_args.input1];
            typename ProtEngine::Wire* input2 = &this->wires[phys.three_args.input2];
            typename ProtEngine::Wire* input3 = &this->wires[phys.three_args.input3];
            BitWidth width = phys.three_args.width;

            typename ProtEngine::Wire selector;
            this->protocol.op_copy(selector, *input3);

            typename ProtEngine::Wire different;
            typename ProtEngine::Wire temp;
            for (BitWidth i = 0; i != width; i++) {
                this->protocol.op_xor(different, input1[i], input2[i]);
                this->protocol.op_and(temp, different, selector);
                this->protocol.op_xor(output[i], temp, input2[i]);
            }
        }

        std::size_t execute_instruction(const PackedPhysInstruction& phys) {
            switch (phys.header.operation) {
            case OpCode::IssueSwapIn:
                this->issue_swap_in(phys.swap.storage, phys.swap.memory);
                return PackedPhysInstruction::size(OpCode::IssueSwapIn);
            case OpCode::IssueSwapOut:
                this->issue_swap_out(phys.swap.memory, phys.swap.storage);
                return PackedPhysInstruction::size(OpCode::IssueSwapOut);
            case OpCode::FinishSwapIn:
                this->wait_for_finish_swap(phys.swap.memory);
                return PackedPhysInstruction::size(OpCode::FinishSwapIn);
            case OpCode::FinishSwapOut:
                this->wait_for_finish_swap(phys.swap.memory);
                return PackedPhysInstruction::size(OpCode::FinishSwapOut);
            case OpCode::CopySwap:
                this->copy_page(phys.swap.storage, phys.swap.memory);
                return PackedPhysInstruction::size(OpCode::CopySwap);
            case OpCode::NetworkPostReceive:
                this->network_post_receive<typename ProtEngine::Wire>(phys.constant.constant, &this->wires[phys.constant.output], phys.constant.width);
                return PackedPhysInstruction::size(OpCode::NetworkPostReceive);
            case OpCode::NetworkFinishReceive:
                this->network_finish_receive(phys.control.data);
                return PackedPhysInstruction::size(OpCode::NetworkFinishReceive);
            case OpCode::NetworkBufferSend:
                this->network_buffer_send<typename ProtEngine::Wire>(phys.constant.constant, &this->wires[phys.constant.output], phys.constant.width);
                return PackedPhysInstruction::size(OpCode::NetworkBufferSend);
            case OpCode::NetworkFinishSend:
                this->network_finish_send(phys.control.data);
                return PackedPhysInstruction::size(OpCode::NetworkFinishSend);
            case OpCode::Input:
                this->protocol.input(&this->wires[phys.no_args.output], phys.no_args.width, (phys.header.flags & FlagEvaluatorInput) == 0);
                return PackedPhysInstruction::size(OpCode::Input);
            case OpCode::Output:
                this->protocol.output(&this->wires[phys.no_args.output], phys.no_args.width);
                return PackedPhysInstruction::size(OpCode::Output);
            case OpCode::PublicConstant:
                this->execute_public_constant(phys);
                return PackedPhysInstruction::size(OpCode::PublicConstant);
            case OpCode::Copy:
                this->execute_copy(phys);
                return PackedPhysInstruction::size(OpCode::Copy);
            case OpCode::IntAdd:
                this->execute_int_add<false>(phys);
                return PackedPhysInstruction::size(OpCode::IntAdd);
            case OpCode::IntAddWithCarry:
                this->execute_int_add<true>(phys);
                return PackedPhysInstruction::size(OpCode::IntAddWithCarry);
            case OpCode::IntIncrement:
                this->execute_int_increment(phys);
                return PackedPhysInstruction::size(OpCode::IntIncrement);
            case OpCode::IntSub:
                this->execute_int_sub(phys);
                return PackedPhysInstruction::size(OpCode::IntSub);
            case OpCode::IntDecrement:
                this->execute_int_decrement(phys);
                return PackedPhysInstruction::size(OpCode::IntDecrement);
            case OpCode::IntMultiply:
                this->execute_int_multiply(phys);
                return PackedPhysInstruction::size(OpCode::IntMultiply);
            case OpCode::IntLess:
                this->execute_int_less(phys);
                return PackedPhysInstruction::size(OpCode::IntLess);
            case OpCode::Equal:
                this->execute_equal(phys);
                return PackedPhysInstruction::size(OpCode::Equal);
            case OpCode::IsZero:
                this->execute_is_zero(phys);
                return PackedPhysInstruction::size(OpCode::IsZero);
            case OpCode::NonZero:
                this->execute_non_zero(phys);
                return PackedPhysInstruction::size(OpCode::NonZero);
            case OpCode::BitNOT:
                this->execute_bit_not(phys);
                return PackedPhysInstruction::size(OpCode::BitNOT);
            case OpCode::BitAND:
                this->execute_bit_and(phys);
                return PackedPhysInstruction::size(OpCode::BitAND);
            case OpCode::BitOR:
                this->execute_bit_or(phys);
                return PackedPhysInstruction::size(OpCode::BitOR);
            case OpCode::BitXOR:
                this->execute_bit_xor(phys);
                return PackedPhysInstruction::size(OpCode::BitXOR);
            case OpCode::ValueSelect:
                this->execute_value_select(phys);
                return PackedPhysInstruction::size(OpCode::ValueSelect);
            default:
                std::abort();
            }
        }

    private:
        ProtEngine& protocol;
        typename ProtEngine::Wire* wires;
        PhysProgramFileReader input;
    };
}

#endif
