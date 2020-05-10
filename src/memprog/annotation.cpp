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

#include "memprog/annotation.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <unordered_map>
#include "memprog/addr.hpp"
#include "memprog/instruction.hpp"
#include "platform/memory.hpp"
#include "util/mapping.hpp"

namespace mage::memprog {
    platform::MappedFile<PackedVirtInstruction> reverse_instructions(const std::string& program, InstructionNumber& inum) {
        platform::MappedFile<ProgramFileHeader> mapping(program.c_str());
        ProgramFileHeader* header = mapping.mapping();

        PackedVirtInstruction* first = reinterpret_cast<PackedVirtInstruction*>(header + 1);
        OutputRange* outputs = header->get_output_ranges();
        std::ptrdiff_t num_instruction_bytes = reinterpret_cast<std::uint8_t*>(outputs) - reinterpret_cast<std::uint8_t*>(first);
        inum = header->num_instructions;

        platform::MappedFile<PackedVirtInstruction> reversed(num_instruction_bytes, true);
        util::reverse_list_into(first, reversed.mapping(), num_instruction_bytes);
        return reversed;
    }

    std::uint64_t reverse_annotate_program(std::ofstream& output, std::string program, PageShift page_shift) {
        InstructionNumber inum;
        platform::MappedFile<PackedVirtInstruction> reversed = reverse_instructions(program, inum);
        PackedVirtInstruction* current = reversed.mapping();

        std::unordered_map<VirtPageNumber, InstructionNumber> next_access;
        std::uint64_t max_working_set_size = 0;

        Annotation ann;
        std::array<VirtPageNumber, 5> vpns;

        do {
            inum--;

            ann.header.num_pages = current->store_page_numbers(vpns.data(), page_shift);
            for (std::uint16_t i = 0; i != ann.header.num_pages; i++) {
                /* Re-profile the code if you modify this inner loop. */
                auto iter = next_access.find(vpns[i]);
                if (iter == next_access.end()) {
                    next_access.insert(std::make_pair(vpns[i], inum));
                    ann.slots[i].next_use = invalid_instr;
                } else {
                    ann.slots[i].next_use = iter->second;
                    iter->second = inum;
                }
            }
            output.write(reinterpret_cast<const char*>(&ann), ann.size());
            max_working_set_size = std::max(max_working_set_size, next_access.size());

            if ((current->header.flags & FlagOutputPageFirstUse) != 0) {
                next_access.erase(pg_num(current->header.output, page_shift));
            }

            current = current->next();
        } while (inum != 0);

        assert(reinterpret_cast<std::uint8_t*>(current) == reinterpret_cast<std::uint8_t*>(reversed.mapping()) + reversed.size());

        return max_working_set_size;
    }

    std::uint64_t reverse_annotate_program(std::string reverse_annotations, std::string program, PageShift page_shift) {
        std::ofstream output;
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output.open(reverse_annotations, std::ios::out | std::ios::binary | std::ios::trunc);
        return reverse_annotate_program(output, program, page_shift);
    }

    std::uint64_t annotate_program(std::string annotations, std::string program, PageShift page_shift) {
        std::ofstream output;
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output.open(annotations, std::ios::out | std::ios::binary | std::ios::trunc);

        std::uint64_t max_ws = reverse_annotate_program(output, program, page_shift);
        output.close();

        platform::MappedFile<Annotation> reversed(annotations.c_str(), false);
        std::filesystem::remove(annotations); // file doesn't die until reversed() goes out of scope
        util::reverse_list_into(reversed.mapping(), annotations.c_str(), reversed.size());

        return max_ws;
    }
}
