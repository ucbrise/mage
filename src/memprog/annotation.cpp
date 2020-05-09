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
#include <cstdint>
#include <algorithm>
#include <array>
#include <fstream>
#include <ostream>
#include <string>
#include <unordered_map>
#include "memprog/addr.hpp"
#include "memprog/instruction.hpp"
#include "platform/memory.hpp"

namespace mage::memprog {
    std::uint64_t reverse_annotate_program(std::string reverse_annotations, std::string program, PageShift page_shift) {
        platform::MappedFile<ProgramFileHeader> mapping(program.c_str());
        ProgramFileHeader* header = mapping.mapping();
        OutputRange* outputs = header->get_output_ranges();

        std::ofstream output;
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output.open(reverse_annotations, std::ios::out | std::ios::binary | std::ios::trunc);

        std::unordered_map<VirtPageNumber, InstructionNumber> next_access;
        std::uint64_t max_working_set_size = 0;

        InstructionNumber inum = header->num_instructions;
        PackedVirtInstruction* current = reinterpret_cast<PackedVirtInstruction*>(outputs);

        Annotation ann;
        std::array<VirtPageNumber, 5> vpns;

        do {
            InstructionFormat format;
            current = current->prev(format);
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
        } while (inum != 0);

        assert(current == reinterpret_cast<PackedVirtInstruction*>(header + 1));

        return max_working_set_size;
    }

    void unreverse_annotations(std::string annotations, std::string reverse_annotations) {
        platform::MappedFile<Annotation> rev(reverse_annotations.c_str(), false);
        platform::MappedFile<std::uint8_t> ann(annotations.c_str(), rev.size());

        const Annotation* source_end = reinterpret_cast<Annotation*>(reinterpret_cast<std::uint8_t*>(rev.mapping()) + rev.size());
        std::uint8_t* target = ann.mapping() + ann.size();
        for (Annotation* source = rev.mapping(); source != source_end; source = source->next()) {
            std::uint16_t size = source->size();
            const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(source);
            target -= size;
            std::copy(ptr, ptr + size, target);
        }
        assert(target == ann.mapping());
    }
}
