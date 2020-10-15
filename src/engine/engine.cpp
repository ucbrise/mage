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

#include "engine/engine.hpp"
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <libaio.h>
#include <chrono>
#include <string>
#include "addr.hpp"
#include "instruction.hpp"
#include "opcode.hpp"
#include "engine/halfgates.hpp"
#include "engine/plaintext.hpp"
#include "platform/filesystem.hpp"

namespace mage::engine {
    template <typename ProtEngine>
    void Engine<ProtEngine>::init(const std::string& storage_file, PageShift shift, std::uint64_t num_pages, std::uint64_t swap_pages, std::uint32_t concurrent_swaps) {
        assert(this->memory == nullptr);

        if (io_setup(concurrent_swaps, &this->aio_ctx) != 0) {
            std::perror("io_setup");
            std::abort();
        }

        this->memory_size = pg_addr(num_pages, shift) * sizeof(typename ProtEngine::Wire);
        this->memory = platform::allocate_resident_memory<typename ProtEngine::Wire>(this->memory_size);
        std::uint64_t required_size = pg_addr(swap_pages, shift) * sizeof(typename ProtEngine::Wire);
        if (storage_file.rfind("/dev/", 0) != std::string::npos) {
            std::uint64_t length;
            this->swapfd = platform::open_file(storage_file.c_str(), &length, true);
            if (length < required_size) {
                std::cerr << "Disk too small: size is " << length << " B, requires " << required_size << " B" << std::endl;
                std::abort();
            }
        } else {
            this->swapfd = platform::create_file(storage_file.c_str(), required_size, true, true);
        }
        this->page_shift = shift;
    }

    template <typename ProtEngine>
    Engine<ProtEngine>::~Engine()  {
        if (this->aio_ctx != 0 && io_destroy(this->aio_ctx) != 0) {
            std::perror("io_destroy");
            std::abort();
        }
        platform::deallocate_resident_memory(this->memory, this->memory_size);
        platform::close_file(this->swapfd);
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_issue_swap_in(const PackedPhysInstruction& phys) {
        StoragePageNumber saddr = pg_addr(phys.swap.storage, this->page_shift);
        PhysPageNumber paddr = pg_addr(phys.swap.memory, this->page_shift);

        auto start = std::chrono::steady_clock::now();
        assert(this->in_flight_swaps.find(phys.swap.memory) == this->in_flight_swaps.end());
        struct iocb& op = this->in_flight_swaps[phys.swap.memory];
        struct iocb* op_ptr = &op;
        io_prep_pread(op_ptr, this->swapfd, &this->memory[paddr], pg_size(this->page_shift) * sizeof(typename ProtEngine::Wire), saddr * sizeof(typename ProtEngine::Wire));
        op_ptr->data = &this->memory[paddr];
        int rv = io_submit(this->aio_ctx, 1, &op_ptr);
        if (rv != 1) {
            std::cerr << "io_submit: " << std::strerror(-rv) << std::endl;
            std::abort();
        }
        // platform::read_from_file_at(this->swapfd, &this->memory[paddr], pg_size(this->page_shift) * sizeof(typename ProtEngine::Wire), saddr * sizeof(typename ProtEngine::Wire));
        auto end = std::chrono::steady_clock::now();
        this->swap_in.event(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_issue_swap_out(const PackedPhysInstruction& phys) {
        PhysPageNumber paddr = pg_addr(phys.swap.memory, this->page_shift);
        StoragePageNumber saddr = pg_addr(phys.swap.storage, this->page_shift);

        auto start = std::chrono::steady_clock::now();
        assert(this->in_flight_swaps.find(phys.swap.memory) == this->in_flight_swaps.end());
        struct iocb& op = this->in_flight_swaps[phys.swap.memory];
        struct iocb* op_ptr = &op;
        io_prep_pwrite(op_ptr, this->swapfd, &this->memory[paddr], pg_size(this->page_shift) * sizeof(typename ProtEngine::Wire), saddr * sizeof(typename ProtEngine::Wire));
        op_ptr->data = &this->memory[paddr];
        int rv = io_submit(this->aio_ctx, 1, &op_ptr);
        if (rv != 1) {
            std::cerr << "io_submit: " << std::strerror(-rv) << std::endl;
            std::abort();
        }
        //platform::write_to_file_at(this->swapfd, &this->memory[paddr], pg_size(this->page_shift) * sizeof(typename ProtEngine::Wire), saddr * sizeof(typename ProtEngine::Wire));
        auto end = std::chrono::steady_clock::now();
        this->swap_out.event(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_copy_swap(const PackedPhysInstruction& phys) {
        PhysPageNumber to_paddr = pg_addr(phys.swap.memory, this->page_shift);
        PhysPageNumber from_paddr = pg_addr(phys.swap.storage, this->page_shift);

        std::copy(&this->memory[from_paddr], &this->memory[from_paddr + pg_size(this->page_shift)], &this->memory[to_paddr]);
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::wait_for_finish_swap(PhysPageNumber ppn) {
        auto iter = this->in_flight_swaps.find(ppn);
        if (iter == this->in_flight_swaps.end()) {
            return;
        }

        auto start = std::chrono::steady_clock::now();

        bool found = false;
        do {
            struct io_event events[aio_process_batch_size];
            int rv = io_getevents(this->aio_ctx, 1, aio_process_batch_size, events, nullptr);
            if (rv < 1) {
                std::cerr << "io_getevents: " << std::strerror(-rv) << std::endl;
                std::abort();
            }
            for (int i = 0; i != rv; i++) {
                struct io_event& event = events[i];
                typename ProtEngine::Wire* page_start = reinterpret_cast<typename ProtEngine::Wire*>(event.data);
                assert(page_start != nullptr);
                PhysPageNumber found_ppn = pg_num(page_start - this->memory, this->page_shift);

                iter = this->in_flight_swaps.find(found_ppn);
                assert(iter != this->in_flight_swaps.end());
                assert(event.obj == &iter->second);
                this->in_flight_swaps.erase(iter);
                if (event.res < 0) {
                    std::cerr << "Swap failed" << std::endl;
                    std::abort();
                }

                found = (found || (found_ppn == ppn));
            }
        } while (!found);

        auto end = std::chrono::steady_clock::now();
        this->swap_blocked.event(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_finish_swap_in(const PackedPhysInstruction& phys) {
        this->wait_for_finish_swap(phys.swap.memory);
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_finish_swap_out(const PackedPhysInstruction& phys) {
        this->wait_for_finish_swap(phys.swap.memory);
    }

    template <typename ProtEngine>
    MessageChannel& Engine<ProtEngine>::contact_worker_checked(WorkerID worker_id) {
        MessageChannel* channel = this->cluster->contact_worker(worker_id);
        if (channel == nullptr) {
            std::cerr << "Attempted to contact worker " << worker_id << std::endl;
            std::abort();
        }
        return *channel;
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_network_post_receive(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* input = &this->memory[phys.constant.output];
        BitWidth num_wires = phys.constant.width;

        MessageChannel& c = this->contact_worker_checked(phys.constant.constant);
        AsyncRead& ar = c.start_post_read();
        ar.into = input;
        ar.length = num_wires * sizeof(typename ProtEngine::Wire);
        c.finish_post_read();
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_network_finish_receive(const PackedPhysInstruction& phys) {
        MessageChannel& c = this->contact_worker_checked(phys.control.data);
        c.wait_until_reads_finished();
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_network_buffer_send(const PackedPhysInstruction& phys) {
        const typename ProtEngine::Wire* output = &this->memory[phys.constant.output];
        BitWidth num_wires = phys.constant.width;

        MessageChannel& c = this->contact_worker_checked(phys.constant.constant);
        typename ProtEngine::Wire* buffer = c.write<typename ProtEngine::Wire>(num_wires);
        std::copy(output, output + num_wires, buffer);
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_network_finish_send(const PackedPhysInstruction& phys) {
        this->contact_worker_checked(phys.control.data).flush();
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_public_constant(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* output = &this->memory[phys.constant.output];
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

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_copy(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* output = &this->memory[phys.one_arg.output];
        typename ProtEngine::Wire* input = &this->memory[phys.one_arg.input1];
        BitWidth width = phys.one_arg.width;

        std::copy(input, input + width, output);
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_int_add(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* output = &this->memory[phys.two_args.output];
        typename ProtEngine::Wire* input1 = &this->memory[phys.two_args.input1];
        typename ProtEngine::Wire* input2 = &this->memory[phys.two_args.input2];
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
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_int_increment(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* output = &this->memory[phys.one_arg.output];
        typename ProtEngine::Wire* input = &this->memory[phys.one_arg.input1];
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

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_int_sub(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* output = &this->memory[phys.two_args.output];
        typename ProtEngine::Wire* input1 = &this->memory[phys.two_args.input1];
        typename ProtEngine::Wire* input2 = &this->memory[phys.two_args.input2];
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

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_int_decrement(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* output = &this->memory[phys.one_arg.output];
        typename ProtEngine::Wire* input = &this->memory[phys.one_arg.input1];
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

    /* Based on https://github.com/samee/obliv-c/blob/obliv-c/src/ext/oblivc/obliv_bits.c */
    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_int_less(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* output = &this->memory[phys.two_args.output];
        typename ProtEngine::Wire* input1 = &this->memory[phys.two_args.input1];
        typename ProtEngine::Wire* input2 = &this->memory[phys.two_args.input2];
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

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_equal(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* output = &this->memory[phys.two_args.output];
        typename ProtEngine::Wire* input1 = &this->memory[phys.two_args.input1];
        typename ProtEngine::Wire* input2 = &this->memory[phys.two_args.input2];
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

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_is_zero(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* output = &this->memory[phys.one_arg.output];
        typename ProtEngine::Wire* input = &this->memory[phys.one_arg.input1];
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

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_non_zero(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* output = &this->memory[phys.one_arg.output];
        typename ProtEngine::Wire* input = &this->memory[phys.one_arg.input1];
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

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_bit_not(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* output = &this->memory[phys.one_arg.output];
        typename ProtEngine::Wire* input = &this->memory[phys.one_arg.input1];
        BitWidth width = phys.one_arg.width;

        for (BitWidth i = 0; i != width; i++) {
            this->protocol.op_not(output[i], input[i]);
        }
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_bit_and(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* output = &this->memory[phys.two_args.output];
        typename ProtEngine::Wire* input1 = &this->memory[phys.two_args.input1];
        typename ProtEngine::Wire* input2 = &this->memory[phys.two_args.input2];
        BitWidth width = phys.two_args.width;

        for (BitWidth i = 0; i != width; i++) {
            this->protocol.op_and(output[i], input1[i], input2[i]);
        }
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_bit_or(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* output = &this->memory[phys.two_args.output];
        typename ProtEngine::Wire* input1 = &this->memory[phys.two_args.input1];
        typename ProtEngine::Wire* input2 = &this->memory[phys.two_args.input2];
        BitWidth width = phys.two_args.width;

        typename ProtEngine::Wire temp1;
        typename ProtEngine::Wire temp2;
        for (BitWidth i = 0; i != width; i++) {
            this->protocol.op_xor(temp1, input1[i], input2[i]);
            this->protocol.op_and(temp2, input1[i], input2[i]);
            this->protocol.op_xor(output[i], temp1, temp2);
        }
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_bit_xor(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* output = &this->memory[phys.two_args.output];
        typename ProtEngine::Wire* input1 = &this->memory[phys.two_args.input1];
        typename ProtEngine::Wire* input2 = &this->memory[phys.two_args.input2];
        BitWidth width = phys.two_args.width;

        for (BitWidth i = 0; i != width; i++) {
            this->protocol.op_xor(output[i], input1[i], input2[i]);
        }
    }

    template <typename ProtEngine>
    void Engine<ProtEngine>::execute_value_select(const PackedPhysInstruction& phys) {
        typename ProtEngine::Wire* output = &this->memory[phys.three_args.output];
        typename ProtEngine::Wire* input1 = &this->memory[phys.three_args.input1];
        typename ProtEngine::Wire* input2 = &this->memory[phys.three_args.input2];
        typename ProtEngine::Wire* input3 = &this->memory[phys.three_args.input3];
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

    template <typename ProtEngine>
    std::size_t Engine<ProtEngine>::execute_instruction(const PackedPhysInstruction& phys) {
        switch (phys.header.operation) {
        case OpCode::IssueSwapIn:
            this->execute_issue_swap_in(phys);
            return PackedPhysInstruction::size(OpCode::IssueSwapIn);
        case OpCode::IssueSwapOut:
            this->execute_issue_swap_out(phys);
            return PackedPhysInstruction::size(OpCode::IssueSwapOut);
        case OpCode::FinishSwapIn:
            this->execute_finish_swap_in(phys);
            return PackedPhysInstruction::size(OpCode::FinishSwapIn);
        case OpCode::FinishSwapOut:
            this->execute_finish_swap_out(phys);
            return PackedPhysInstruction::size(OpCode::FinishSwapOut);
        case OpCode::CopySwap:
            this->execute_copy_swap(phys);
            return PackedPhysInstruction::size(OpCode::CopySwap);
        case OpCode::NetworkPostReceive:
            this->execute_network_post_receive(phys);
            return PackedPhysInstruction::size(OpCode::NetworkPostReceive);
        case OpCode::NetworkFinishReceive:
            this->execute_network_finish_receive(phys);
            return PackedPhysInstruction::size(OpCode::NetworkFinishReceive);
        case OpCode::NetworkBufferSend:
            this->execute_network_buffer_send(phys);
            return PackedPhysInstruction::size(OpCode::NetworkBufferSend);
        case OpCode::NetworkFinishSend:
            this->execute_network_finish_send(phys);
            return PackedPhysInstruction::size(OpCode::NetworkFinishSend);
        case OpCode::Input:
            this->protocol.input(&this->memory[phys.no_args.output], phys.no_args.width, (phys.header.flags & FlagEvaluatorInput) == 0);
            return PackedPhysInstruction::size(OpCode::Input);
        case OpCode::Output:
            this->protocol.output(&this->memory[phys.no_args.output], phys.no_args.width);
            return PackedPhysInstruction::size(OpCode::Output);
        case OpCode::PublicConstant:
            this->execute_public_constant(phys);
            return PackedPhysInstruction::size(OpCode::PublicConstant);
        case OpCode::Copy:
            this->execute_copy(phys);
            return PackedPhysInstruction::size(OpCode::Copy);
        case OpCode::IntAdd:
            this->execute_int_add(phys);
            return PackedPhysInstruction::size(OpCode::IntAdd);
        case OpCode::IntIncrement:
            this->execute_int_increment(phys);
            return PackedPhysInstruction::size(OpCode::IntIncrement);
        case OpCode::IntSub:
            this->execute_int_sub(phys);
            return PackedPhysInstruction::size(OpCode::IntSub);
        case OpCode::IntDecrement:
            this->execute_int_decrement(phys);
            return PackedPhysInstruction::size(OpCode::IntDecrement);
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

    /* Explicitly instantiate Engine template for each protocol. */
    template class Engine<PlaintextEvaluationEngine>;
    template class Engine<HalfGatesGarblingEngine>;
    template class Engine<HalfGatesEvaluationEngine>;
}
