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
  * @file engine/addmultiply.hpp
  * @brief Add-Multiply Engine, for protocols that support element-wise Add
  * and Multiply operations on encrypted batches of numbers.
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
    /**
     * @brief Checks if an instruction operates on normalized ciphertexts.
     *
     * @param phys The specified instruction.
     * @return True if the instruction operates on normalized ciphertexts,
     * otherwise false.
     */
    static inline bool normalized(const PackedPhysInstruction& phys) {
        return (phys.header.flags & FlagNotNormalized) == 0;
    }

    /**
     * @brief AddMultiplyEngine is an engine for protocols that support
     * addition and multiplications on batches of values in a SIMD fashion.
     * An example of such a protocol is Leveled Homomorphic Encryption.
     *
     * @tparam ProtEngine The type of the underlying protocol driver.
     */
    template <typename ProtEngine>
    class AddMultiplyEngine : private Engine {
    public:
        /**
         * @brief Creates an Add-Multiply Engine.
         *
         * @param network This worker's network endpoint for intra-party
         * communication.
         * @param worker Value in the configuration file holding the storage
         * file/device path for this worker.
         * @param prot The protocol driver to use.
         * @param program The file path of the memory program to execute.
         */
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

        /**
         * @brief Execute the memory program.
         */
        void execute_program() {
            InstructionNumber num_instructions = this->input.get_header().num_instructions;
            this->progress_bar.set_label("Execution");
            this->input.set_progress_bar(&this->progress_bar);
            for (InstructionNumber i = 0; i != num_instructions; i++) {
                PackedPhysInstruction& phys = this->input.start_instruction();
                std::size_t size = this->execute_instruction(phys);
                this->input.finish_instruction(size);
            }
            this->progress_bar.finish();
        }

    private:
        /**
         * @brief Execute the provided instruction of the memory program.
         *
         * @param phys The instruction to execute.
         * @return The size of the instruction's encoding, in bytes.
         */
        std::size_t execute_instruction(const PackedPhysInstruction& phys) {
            switch (phys.header.operation) {
            case OpCode::PrintStats:
                std::cout << this->input.get_stats() << std::endl;
                this->print_stats();
                this->protocol.print_stats();
                return PackedPhysInstruction::size(OpCode::PrintStats);
            case OpCode::StartTimer:
                this->start_timer();
                return PackedPhysInstruction::size(OpCode::StartTimer);
            case OpCode::StopTimer:
                this->stop_timer();
                return PackedPhysInstruction::size(OpCode::StartTimer);
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
                this->network_post_receive(phys.constant.constant, &this->memory[phys.constant.output], ProtEngine::ciphertext_size(phys.constant.width, normalized(phys)));
                return PackedPhysInstruction::size(OpCode::NetworkPostReceive);
            case OpCode::NetworkFinishReceive:
                this->network_finish_receive(phys.control.data);
                return PackedPhysInstruction::size(OpCode::NetworkFinishReceive);
            case OpCode::NetworkBufferSend:
                this->network_buffer_send(phys.constant.constant, &this->memory[phys.constant.output], ProtEngine::ciphertext_size(phys.constant.width, normalized(phys)));
                return PackedPhysInstruction::size(OpCode::NetworkBufferSend);
            case OpCode::NetworkFinishSend:
                this->network_finish_send(phys.control.data);
                return PackedPhysInstruction::size(OpCode::NetworkFinishSend);
            case OpCode::Input:
                this->protocol.input(&this->memory[phys.no_args.output], phys.no_args.width, normalized(phys));
                return PackedPhysInstruction::size(OpCode::Input);
            case OpCode::Output:
                this->protocol.output(&this->memory[phys.no_args.output], phys.no_args.width, normalized(phys));
                return PackedPhysInstruction::size(OpCode::Output);
            case OpCode::IntAdd:
                this->protocol.op_add(&this->memory[phys.two_args.output], &this->memory[phys.two_args.input1], &this->memory[phys.two_args.input2], phys.two_args.width, normalized(phys));
                return PackedPhysInstruction::size(OpCode::IntAdd);
            case OpCode::IntSub:
                this->protocol.op_sub(&this->memory[phys.two_args.output], &this->memory[phys.two_args.input1], &this->memory[phys.two_args.input2], phys.two_args.width, normalized(phys));
                return PackedPhysInstruction::size(OpCode::IntSub);
            case OpCode::IntMultiply:
                this->protocol.op_multiply(&this->memory[phys.two_args.output], &this->memory[phys.two_args.input1], &this->memory[phys.two_args.input2], phys.two_args.width);
                return PackedPhysInstruction::size(OpCode::IntMultiply);
            case OpCode::MultiplyPlaintext:
                this->protocol.op_multiply_plaintext(&this->memory[phys.two_args.output], &this->memory[phys.two_args.input1], &this->memory[phys.two_args.input2], phys.two_args.width);
                return PackedPhysInstruction::size(OpCode::MultiplyPlaintext);
            case OpCode::SwitchLevel:
                this->protocol.op_switch_level(&this->memory[phys.one_arg.output], &this->memory[phys.one_arg.input1], phys.one_arg.width);
                return PackedPhysInstruction::size(OpCode::SwitchLevel);
            case OpCode::MultiplyRaw:
                this->protocol.op_multiply_raw(&this->memory[phys.two_args.output], &this->memory[phys.two_args.input1], &this->memory[phys.two_args.input2], phys.two_args.width);
                return PackedPhysInstruction::size(OpCode::MultiplyRaw);
            case OpCode::MultiplyPlaintextRaw:
                this->protocol.op_multiply_plaintext_raw(&this->memory[phys.two_args.output], &this->memory[phys.two_args.input1], &this->memory[phys.two_args.input2], phys.two_args.width);
                return PackedPhysInstruction::size(OpCode::MultiplyPlaintextRaw);
            case OpCode::Renormalize:
                this->protocol.op_normalize(&this->memory[phys.one_arg.output], &this->memory[phys.one_arg.input1], phys.one_arg.width);
                return PackedPhysInstruction::size(OpCode::Renormalize);
            case OpCode::Encode:
                this->protocol.op_encode(&this->memory[phys.constant.output], phys.constant.constant, phys.constant.width);
                return PackedPhysInstruction::size(OpCode::Encode);
            default:
                std::cerr << "Instruction " << opcode_to_string(phys.header.operation) << " is not supported." << std::endl;
                std::abort();
            }
        }

        ProtEngine& protocol;
        std::uint8_t* memory;
        PhysProgramFileReader input;
    };
}

#endif
