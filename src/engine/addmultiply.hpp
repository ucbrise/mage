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

#ifndef MAGE_ENGINE_ADDMULTIPLY_HPP_
#define MAGE_ENGINE_ADDMULTIPLY_HPP_

#include <iostream>
#include <memory>
#include <string>
#include "addr.hpp"
#include "opcode.hpp"
#include "engine/engine.hpp"
#include "programfile.hpp"
#include "util/config.hpp"
#include "util/misc.hpp"
#include "util/stats.hpp"

namespace mage::engine {
    template <typename ProtEngine>
    class AddMultiplyEngine : private Engine {
    public:
        AddMultiplyEngine(const std::shared_ptr<ClusterNetwork>& network, const util::ConfigValue& worker, ProtEngine& prot, std::string program)
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
            this->memory = this->get_memory();
        }

        void execute_program() {
            InstructionNumber num_instructions = this->input.get_header().num_instructions;
            for (InstructionNumber i = 0; i != num_instructions; i++) {
                PackedPhysInstruction& phys = this->input.start_instruction();
                std::size_t size = this->execute_instruction(phys);
                this->input.finish_instruction(size);
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
                this->network_post_receive(phys.constant.constant, &this->memory[phys.constant.output], ProtEngine::level_size(phys.constant.width));
                return PackedPhysInstruction::size(OpCode::NetworkPostReceive);
            case OpCode::NetworkFinishReceive:
                this->network_finish_receive(phys.control.data);
                return PackedPhysInstruction::size(OpCode::NetworkFinishReceive);
            case OpCode::NetworkBufferSend:
                this->network_buffer_send(phys.constant.constant, &this->memory[phys.constant.output], ProtEngine::level_size(phys.constant.width));
                return PackedPhysInstruction::size(OpCode::NetworkBufferSend);
            case OpCode::NetworkFinishSend:
                this->network_finish_send(phys.control.data);
                return PackedPhysInstruction::size(OpCode::NetworkFinishSend);
            case OpCode::Input:
                this->protocol.input(&this->memory[phys.no_args.output], phys.no_args.width);
                return PackedPhysInstruction::size(OpCode::Input);
            case OpCode::Output:
                this->protocol.output(&this->memory[phys.no_args.output], phys.no_args.width);
                return PackedPhysInstruction::size(OpCode::Output);
            // case OpCode::PublicConstant:
            //     this->execute_public_constant(phys);
            //     return PackedPhysInstruction::size(OpCode::PublicConstant);
            // case OpCode::Copy:
            //     this->execute_copy(phys);
            //     return PackedPhysInstruction::size(OpCode::Copy);
            case OpCode::IntAdd:
                this->protocol.op_add(&this->memory[phys.two_args.output], &this->memory[phys.two_args.input1], &this->memory[phys.two_args.input2], phys.two_args.width);
                return PackedPhysInstruction::size(OpCode::IntAdd);
            // case OpCode::IntAddWithCarry:
            //     this->execute_int_add<true>(phys);
            //     return PackedPhysInstruction::size(OpCode::IntAddWithCarry);
            // case OpCode::IntIncrement:
            //     this->execute_int_increment(phys);
            //     return PackedPhysInstruction::size(OpCode::IntIncrement);
            // case OpCode::IntSub:
            //     this->execute_int_sub(phys);
            //     return PackedPhysInstruction::size(OpCode::IntSub);
            // case OpCode::IntDecrement:
            //     this->execute_int_decrement(phys);
            //     return PackedPhysInstruction::size(OpCode::IntDecrement);
            case OpCode::IntMultiply:
                this->protocol.op_multiply(&this->memory[phys.two_args.output], &this->memory[phys.two_args.input1], &this->memory[phys.two_args.input2], phys.two_args.width);
                return PackedPhysInstruction::size(OpCode::IntMultiply);
            // case OpCode::IntLess:
            //     this->execute_int_less(phys);
            //     return PackedPhysInstruction::size(OpCode::IntLess);
            // case OpCode::Equal:
            //     this->execute_equal(phys);
            //     return PackedPhysInstruction::size(OpCode::Equal);
            // case OpCode::IsZero:
            //     this->execute_is_zero(phys);
            //     return PackedPhysInstruction::size(OpCode::IsZero);
            // case OpCode::NonZero:
            //     this->execute_non_zero(phys);
            //     return PackedPhysInstruction::size(OpCode::NonZero);
            // case OpCode::BitNOT:
            //     this->execute_bit_not(phys);
            //     return PackedPhysInstruction::size(OpCode::BitNOT);
            // case OpCode::BitAND:
            //     this->execute_bit_and(phys);
            //     return PackedPhysInstruction::size(OpCode::BitAND);
            // case OpCode::BitOR:
            //     this->execute_bit_or(phys);
            //     return PackedPhysInstruction::size(OpCode::BitOR);
            // case OpCode::BitXOR:
            //     this->execute_bit_xor(phys);
            //     return PackedPhysInstruction::size(OpCode::BitXOR);
            // case OpCode::ValueSelect:
            //     this->execute_value_select(phys);
            //     return PackedPhysInstruction::size(OpCode::ValueSelect);
            default:
                std::cerr << "Instruction " << opcode_to_string(phys.header.operation) << " is not supported." << std::endl;
                std::abort();
            }
        }

    private:
        ProtEngine& protocol;
        std::uint8_t* memory;
        PhysProgramFileReader input;
    };
}

#endif
