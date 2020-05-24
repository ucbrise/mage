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
    struct ProgramFileHeader {
        InstructionNumber num_instructions;
        std::uint64_t num_pages;
        std::uint64_t num_swap_pages;
        std::uint32_t max_concurrent_swaps;
        PageShift page_shift;
    };

    template <std::uint8_t addr_bits, std::uint8_t storage_bits, bool backwards_readable>
    class ProgramFileWriter : private util::BufferedFileWriter<backwards_readable> {
    public:
        ProgramFileWriter(std::string filename, PageShift shift = 0, std::uint64_t num_pages = 0)
            : util::BufferedFileWriter<backwards_readable>(filename.c_str()), instruction_count(0), page_shift(shift), page_count(num_pages), swap_page_count(0), concurrent_swaps(1) {
            ProgramFileHeader header = { 0 };
            platform::write_to_file(this->fd, &header, sizeof(header));
        }

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

        std::uint64_t num_instructions() const {
            return this->instruction_count;
        }

        void set_page_count(std::uint64_t num_pages) {
            this->page_count = num_pages;
        }

        void set_swap_page_count(std::uint64_t num_swap_pages) {
            this->swap_page_count = num_swap_pages;
        }

        void set_concurrent_swaps(std::uint32_t max_concurrent_swaps) {
            this->concurrent_swaps = max_concurrent_swaps;
        }

        void set_page_shift(PageShift shift) {
            this->page_shift = shift;
        }

        PackedInstruction<addr_bits, storage_bits>& start_instruction(std::size_t maximum_size = sizeof(PackedInstruction<addr_bits, storage_bits>)) {
            return this->template start_write<PackedInstruction<addr_bits, storage_bits>>(maximum_size);
        }

        void finish_instruction(std::size_t actual_size) {
            this->finish_write(actual_size);
            this->instruction_count++;
        }

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

    template <std::uint8_t addr_bits, std::uint8_t storage_bits, bool backwards_readable>
    class ProgramFileReader : private util::BufferedFileReader<backwards_readable> {
    public:
        ProgramFileReader(std::string filename) : util::BufferedFileReader<backwards_readable>(filename.c_str()) {
            platform::read_from_file(this->fd, &this->header, sizeof(this->header));
        }

        PackedInstruction<addr_bits, storage_bits>& start_instruction(std::size_t maximum_size = sizeof(PackedInstruction<addr_bits, storage_bits>)) {
            return this->template start_read<PackedInstruction<addr_bits, storage_bits>>(maximum_size);
        }

        void finish_instruction(std::size_t actual_size) {
            this->finish_read(actual_size);
        }

        const ProgramFileHeader& get_header() const {
            return this->header;
        }

    private:
        ProgramFileHeader header;
    };

    template <std::uint8_t addr_bits, std::uint8_t storage_bits, bool backwards_readable>
    class ProgramReverseFileReader : private util::BufferedReverseFileReader<backwards_readable> {
    public:
        ProgramReverseFileReader(std::string filename) : util::BufferedReverseFileReader<backwards_readable>(filename.c_str()) {
            platform::seek_file(this->fd, 0);
            platform::read_from_file(this->fd, &this->header, sizeof(this->header));
        }

        PackedInstruction<addr_bits, storage_bits>& read_instruction(std::size_t& size) {
            return this->template read<PackedInstruction<addr_bits, storage_bits>>(size);
        }

        const ProgramFileHeader& get_header() const {
            return this->header;
        }

    private:
        ProgramFileHeader header;
    };

    using VirtProgramFileWriter = ProgramFileWriter<virtual_address_bits, virtual_address_bits, true>;
    using VirtProgramFileReader = ProgramFileReader<virtual_address_bits, virtual_address_bits, true>;
    using VirtProgramReverseFileReader = ProgramReverseFileReader<virtual_address_bits, virtual_address_bits, true>;

    using PhysProgramFileWriter = ProgramFileWriter<physical_address_bits, storage_address_bits, false>;
    using PhysProgramFileReader = ProgramFileReader<physical_address_bits, storage_address_bits, false>;
}

#endif
