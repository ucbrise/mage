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
 * @file programfile.hpp
 * @brief Tools for reading and writing MAGE's bytecodes.
 */

#ifndef MAGE_PROGRAMFILE_HPP_
#define MAGE_PROGRAMFILE_HPP_

#include <cstdint>

#include <string>
#include <vector>

#include "addr.hpp"
#include "instruction.hpp"
#include "util/filebuffer.hpp"
#include "platform/filesystem.hpp"

namespace mage {
    /**
     * @brief Header containing metadata at the start of any of MAGE's
     * bytecodes.
     */
    struct ProgramFileHeader {
        InstructionNumber num_instructions;
        std::uint64_t num_pages;
        std::uint64_t num_swap_pages;
        std::uint32_t max_concurrent_swaps;
        PageShift page_shift;
    };

    /**
     * @brief Tool for writing a bytecode (sometimes referred to as a
     * program file).
     *
     * @tparam addr_bits,storage_bits Parameters of the type of instruction
     * being written.
     * @tparam backwards_readable True if size markers should be written to
     * allow instructions to be read in reverse order, otherwise false.
     */
    template <std::uint8_t addr_bits, std::uint8_t storage_bits, bool backwards_readable>
    class ProgramFileWriter : private util::BufferedFileWriter<backwards_readable> {
    public:
        /**
         * @brief Creates a file and creates a ProgramFileWriter set up to
         * write to that file, leaving space for the metadata header, which
         * will be filled in later.
         *
         * @param filename Name of the file to create and write to.
         * @param shift Describes the page size, and is included in the
         * metadata header (can be set later on).
         * @num_pages The number of pages of the address space used by this
         * bytecode program, included in the metadata header (can be set later
         * on).
         */
        ProgramFileWriter(std::string filename, PageShift shift = 0, std::uint64_t num_pages = 0)
            : util::BufferedFileWriter<backwards_readable>(filename.c_str()), instruction_count(0), page_shift(shift), page_count(num_pages), swap_page_count(0), concurrent_swaps(1) {
            ProgramFileHeader header = { 0 };
            platform::write_to_file(this->fd, &header, sizeof(header));
        }

        /**
         * @brief Writes any remaining buffered data to the file, and then
         * writes the metadata header to the beginning of the file.
         */
        virtual ~ProgramFileWriter() {
            this->flush();
            platform::seek_file(this->fd, 0);

            ProgramFileHeader header = { 0 };
            header.num_instructions = this->instruction_count;
            header.num_pages = this->page_count;
            header.num_swap_pages = this->swap_page_count;
            header.max_concurrent_swaps = this->concurrent_swaps;
            header.page_shift = this->page_shift;
            platform::write_to_file(this->fd, &header, sizeof(header));
        }

        /**
         * @brief Obtains the number of instructions written to the file so
         * far.
         *
         * @return The number of instructions written to the file so far.
         */
        std::uint64_t num_instructions() const {
            return this->instruction_count;
        }

        /**
         * @brief Sets the number of memory pages used by this program, which
         * is written to the file as part of the metadata header.
         *
         * @param num_pages The number of memory pages used by this program.
         */
        void set_page_count(std::uint64_t num_pages) {
            this->page_count = num_pages;
        }

        /**
         * @brief Sets the number of swap pages used by this program, which
         * is written to the file as part of the metadata header.
         *
         * @param num_swap_pages The number of swap pages uesd by this program.
         */
        void set_swap_page_count(std::uint64_t num_swap_pages) {
            this->swap_page_count = num_swap_pages;
        }

        /**
         * @brief Sets the maximum number of swap operations that may happen
         * concurrently in the program, which is written to the file as part of
         * the metadata header.
         *
         * @param max_concurrent_swaps The maximum number of swap operations
         * that may happen concurrently in the program.
         */
        void set_concurrent_swaps(std::uint32_t max_concurrent_swaps) {
            this->concurrent_swaps = max_concurrent_swaps;
        }

        /**
         * @brief Sets the page shift, describing the page size, which is
         * written to the file as part of the metadata header.
         *
         * @param shift The page shift.
         */
        void set_page_shift(PageShift shift) {
            this->page_shift = shift;
        }

        /**
         * @brief Allocates space in the output buffer for a new instruction
         * and returns a reference to it so that a caller can initialize it.
         *
         * @param maximum_size An upper bound on the size of the instruction.
         * @return A reference to the allocated space that should be
         * initialized with the new instruction.
         */
        PackedInstruction<addr_bits, storage_bits>& start_instruction(std::size_t maximum_size = sizeof(PackedInstruction<addr_bits, storage_bits>)) {
            return this->template start_write<PackedInstruction<addr_bits, storage_bits>>(maximum_size);
        }

        /**
         * @brief Commits the next instruction, allocated with
         * start_instruction() and initialized by the caller, to the output
         * bytecode.
         *
         * @param actual_size The size of the instruction to be committed,
         * which may be less than the size allocated by start_instruction().
         */
        void finish_instruction(std::size_t actual_size) {
            this->finish_write(actual_size);
            this->instruction_count++;
        }

        /**
         * @brief Packs (encodes) and appends the specified logical instruction
         * to the output bytecode.
         *
         * @brief v The specified logical instruction.
         */
        void append_instruction(const Instruction& v) {
            auto& packed = this->start_instruction();
            std::size_t size = v.pack(packed);
            this->finish_instruction(size);
        }

    private:
        std::uint64_t instruction_count;
        std::uint64_t page_count;
        std::uint64_t swap_page_count;
        std::uint32_t concurrent_swaps;
        PageShift page_shift;
    };

    /**
     * @brief Tool for reading a bytecode (sometimes referred to as a
     * program file).
     *
     * @tparam addr_bits,storage_bits Parameters of the type of instruction
     * being read.
     * @tparam backwards_readable True if size markers are present in the file
     * to allow instructions to be read in reverse order, otherwise false.
     */
    template <std::uint8_t addr_bits, std::uint8_t storage_bits, bool backwards_readable>
    class ProgramFileReader : private util::BufferedFileReader<backwards_readable> {
    public:
        /**
         * @brief Opens a file containing a MAGE bytecode program and creates
         * a ProgramFileReader to read its instructions.
         *
         * @param filename The name of the file containing the MAGE bytecode
         * program to read.
         */
        ProgramFileReader(std::string filename) : util::BufferedFileReader<backwards_readable>(filename.c_str()) {
            platform::read_from_file(this->fd, &this->header, sizeof(this->header));
        }

        /**
         * @brief Enables collection of statistics for rebuffer times.
         *
         * @param label Label to use for statistics collection.
         */
        void enable_stats(const std::string& label) {
            this->util::BufferedFileReader<backwards_readable>::enable_stats(label);
        }

        /**
         * @brief Reads data containing the next instruction into a local
         * buffer and return a reference to it.
         *
         * @param maximum_size An upper bound on the size of the next
         * instruction.
         * @return A reference to a local buffer containing the next
         * instruction.
         */
        PackedInstruction<addr_bits, storage_bits>& start_instruction(std::size_t maximum_size = sizeof(PackedInstruction<addr_bits, storage_bits>)) {
            return this->template start_read<PackedInstruction<addr_bits, storage_bits>>(maximum_size);
        }

        /**
         * @brief Marks the instruction obtained by the previous call to
         * start_instruction() as read, moving on to the next instruction.
         *
         * @param actual_size The actual size of the read instruction, which
         * may be smaller than the maximum size given to start_instruction().
         */
        void finish_instruction(std::size_t actual_size) {
            this->finish_read(actual_size);
        }

        /**
         * @brief Obtains the metadata header for the bytecode program being
         * read by this ProgramFileReader.
         *
         * @return A const reference to the bytecode program's metadata header.
         */
        const ProgramFileHeader& get_header() const {
            return this->header;
        }

        /**
         * @brief Returns a reference to the statistics collector for this
         * ProgramFileReader instance.
         */
        util::StreamStats& get_stats() {
            return this->util::BufferedFileReader<backwards_readable>::get_stats();
        }

    private:
        ProgramFileHeader header;
    };

    /**
     * @brief Tool for reading a byte (sometimes referred to as a program file)
     * in reverse order.
     *
     * This requires the program file to have been written with size markers
     * allowing the instructions to be read in reverse order.
     *
     * @tparam addr_bits,storage_bits Parameters of the type of instruction
     * being read.
     * @tparam backwards_readable Must be true.
     */
    template <std::uint8_t addr_bits, std::uint8_t storage_bits, bool backwards_readable>
    class ProgramReverseFileReader : private util::BufferedReverseFileReader<backwards_readable> {
    public:
        /**
         * @brief Opens a file containing a MAGE bytecode program and creates
         * a ProgramReverseFileReader to read its instructions in reverse
         * order.
         *
         * @param filename The name of the file containing the MAGE bytecode
         * program to read.
         */
        ProgramReverseFileReader(std::string filename) : util::BufferedReverseFileReader<backwards_readable>(filename.c_str()) {
            platform::seek_file(this->fd, 0);
            platform::read_from_file(this->fd, &this->header, sizeof(this->header));
        }

        /**
         * @brief Reads data containing the next instruction (in reverse order)
         * into a local buffer and returns a reference to it, advancing to the
         * next instruction in reverse order.
         *
         * @param[out] The size of the instruction, in bytes.
         * @return A reference to a local buffer containing the next
         * instruction (in reverse order).
         */
        PackedInstruction<addr_bits, storage_bits>& read_instruction(std::size_t& size) {
            return this->template read<PackedInstruction<addr_bits, storage_bits>>(size);
        }

        /**
         * @brief Obtains the metadata header for the bytecode program being
         * read by this ProgramReverseFileReader.
         *
         * @return A const reference to the bytecode program's metadata header.
         */
        const ProgramFileHeader& get_header() const {
            return this->header;
        }

    private:
        ProgramFileHeader header;
    };

    /**
     * @brief Instantiation of the ProgramFileWriter template to write
     * instructions referencing virtual addresses (virtual byte code).
     *
     * The resulting program file is readable in reverse order.
     */
    using VirtProgramFileWriter = ProgramFileWriter<virtual_address_bits, virtual_address_bits, true>;

    /**
     * @brief Instantiation of the ProgramFileReader template to read
     * instructions referencing virtual addresses (virtual byte code).
     */
    using VirtProgramFileReader = ProgramFileReader<virtual_address_bits, virtual_address_bits, true>;

    /**
     * @brief Instantiation of the ProgramFileReader template to read
     * instructions referencing virtual addresses (virtual byte code) in
     * reverse order.
     */
    using VirtProgramReverseFileReader = ProgramReverseFileReader<virtual_address_bits, virtual_address_bits, true>;

    /**
     * @brief Instantiation of the ProgramFileWriter template to write
     * instructions referencing physical addresses (physical byte code).
     *
     * The resulting program file is not readable in reverse order.
     */
    using PhysProgramFileWriter = ProgramFileWriter<physical_address_bits, storage_address_bits, false>;

     /**
      * @brief Instantiation of the ProgramFileReader template to read
      * instructions referencing physical addresses (physical byte code).
      */
     using PhysProgramFileReader = ProgramFileReader<physical_address_bits, storage_address_bits, false>;
}

#endif
