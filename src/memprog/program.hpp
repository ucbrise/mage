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
 * @file memprog/program.hpp
 * @brief Program object used for DSL execution during MAGE's planning phase
 *
 * The program object provides an interface between the DSL and MAGE's
 * placement module, allowing them to interact as the DSL executes in MAGE's
 * planner.
 */

#ifndef MAGE_MEMPROG_PROGRAM_HPP_
#define MAGE_MEMPROG_PROGRAM_HPP_

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include "addr.hpp"
#include "instruction.hpp"
#include "memprog/placement.hpp"
#include "opcode.hpp"
#include "programfile.hpp"

namespace mage::memprog {
    /**
     * @brief Used by DSLs to emit virtual bytecode instructions and interact
     * with MAGE's planner's placement module as they execute.
     *
     * @tparam Placer The type of placement module used by MAGE's planner.
     */
    template <typename Placer>
    class Program : public VirtProgramFileWriter {
    public:
        /**
         * @brief Creates a @p Program object that outputs virtual bytecode to
         * the specified file,
         *
         * @param filename The name of the file to which to output the virtual
         * bytecode.
         * @param shift Base-2 logarithm of the page size.
         * @param prot Plugin with sizing information specific to the target
         * protocol, used for placement.
         */
        Program(std::string filename, PageShift shift, PlacementPlugin prot) : VirtProgramFileWriter(filename, shift), placer(shift), protocol(prot) {
        }

        /**
         * @brief Destructor.
         */
        ~Program() {
            this->set_page_count(this->placer.get_num_pages());
            if (Program<Placer>::current_working_program == this) {
                Program<Placer>::current_working_program = nullptr;
            }
        }

        /**
         * @brief Obtains a reference to a buffer for the next instruction to
         * output to the virtual bytecode.
         *
         * The caller should populate the buffer with the next instruction to
         * write out, and then call @p commit_instruction.
         *
         * @return A reference to a buffer for the caller to populate with the
         * next instruction to write out to the virtual byte code.
         */
        Instruction& instruction() {
            return this->current;
        }

        /**
         * @brief Commits the current instruction (which should have been
         * populated via @p instruction function) to the output virtual
         * bytecode.
         *
         * If @p output_width is nonzero, then this function will allocate
         * space for the instruction's output and populate its output address,
         * according to the specified @p output_width.
         *
         * @param output_width The width of the output in the MAGE-virtual
         * address space, or 0 if the output address need not be populated by
         * this function.
         * @return The address of the instruction's output field.
         */
        VirtAddr commit_instruction(memprog::AllocationSize output_width) {
            if (output_width != 0) {
                bool fresh_page;
                this->current.header.output = this->placer.allocate_virtual(output_width, fresh_page);
                if (fresh_page) {
                    this->current.header.flags |= FlagOutputPageFirstUse;
                }
            }
            assert(this->current.header.output != invalid_vaddr);
            this->append_instruction(this->current);
            return this->current.header.output;
        }

        /**
         * @brief Deallocates some MAGE-virtual memory, marking it as no longer
         * needed. The same memory may be returned as part of a future
         * allocation.
         *
         * @param addr The address of the memory to deallocate.
         * @param width The amount of memory to deallocate.
         */
        void recycle(VirtAddr addr, memprog::AllocationSize width) {
            this->placer.deallocate_virtual(addr, width);
        }

        /**
         * @brief Inserts an instruction into the virtual bytecode to flush any
         * network buffers for sending data to the specified worker.
         *
         * @param to The ID of the specified worker.
         */
        void finish_send(WorkerID to) {
            Instruction instr;
            instr.header.operation = OpCode::NetworkFinishSend;
            instr.header.flags = 0;
            instr.control.data = to;
            this->append_instruction(instr);
        }

        /**
         * @brief Inserts an instruction into the virtual bytecode to wait for
         * any outstanding network receive operations from the specified
         * worker to complete.
         *
         * @param to The ID of the specified worker.
         */
        void finish_receive(WorkerID from) {
            Instruction instr;
            instr.header.operation = OpCode::NetworkFinishReceive;
            instr.header.flags = 0;
            instr.control.data = from;
            this->append_instruction(instr);
        }

        /**
         * @brief Inserts an instruction into the virtual bytecode to print out
         * peformance statistics.
         */
        void print_stats() {
            Instruction instr;
            instr.header.operation = OpCode::PrintStats;
            instr.header.flags = 0;
            instr.control.data = 0;
            this->append_instruction(instr);
        }

        /**
         * @brief Inserts a "start timer" instruction into the virtual
         * bytecode.
         */
        void start_timer() {
            Instruction instr;
            instr.header.operation = OpCode::StartTimer;
            instr.header.flags = 0;
            instr.control.data = 0;
            this->append_instruction(instr);
        }

        /**
         * @brief Inserts an instruction into the virtual bytecode to print out
         * the time elapsed since the last "start timer" instruction.
         */
        void stop_timer() {
            Instruction instr;
            instr.header.operation = OpCode::StopTimer;
            instr.header.flags = 0;
            instr.control.data = 0;
            this->append_instruction(instr);
        }

        /**
         * @brief Uses the protocol plugin to obtain the amount of space
         * required in the MAGE-virtual address space to store a variable of
         * the specified size and type.
         *
         * @param logical_width The logical size of the variable.
         * @param type The logical type of the variable.
         * @return The size of the variable in the MAGE-virtual address space.
         */
        memprog::AllocationSize get_physical_width(std::uint64_t logical_width, memprog::PlaceableType type) const {
            return this->protocol(logical_width, type);
        }

        /**
         * @brief Sets the "current working program", a global variable that
         * some DSLs may use.
         *
         * @param cwp The previous "current working program."
         */
        static Program<Placer>* set_current_working_program(Program<Placer>* cwp) {
            Program<Placer>* old_cwp = Program<Placer>::current_working_program;
            Program<Placer>::current_working_program = cwp;
            return old_cwp;
        }

        /**
         * @brief Obtains the "current working program", a global variable that
         * some DSLs may use.
         *
         * @return The current "current working program."
         */
        static Program<Placer>* get_current_working_program() {
            return Program<Placer>::current_working_program;
        }

    private:
        Instruction current;
        Placer placer;
        PlacementPlugin protocol;
        static Program<Placer>* current_working_program;
    };

    template <typename Placer>
    Program<Placer>* Program<Placer>::current_working_program = nullptr;

    /**
     * @brief The @p Program type to use with MAGE's default planning pipeline.
     */
    using DefaultProgram = Program<BinnedPlacer>;
}

#endif
