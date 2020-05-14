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
#include <string>
#include <unordered_map>
#include "addr.hpp"
#include "instruction.hpp"
#include "programfile.hpp"
#include "platform/memory.hpp"
#include "util/filebuffer.hpp"

namespace mage::memprog {
    std::uint64_t annotate_program(util::BufferedFileWriter<true>& output, std::string program, PageShift page_shift) {
        VirtProgramReverseFileReader instructions(program);
        InstructionNumber inum = instructions.get_header().num_instructions;

        std::unordered_map<VirtPageNumber, InstructionNumber> next_access;
        std::uint64_t max_working_set_size = 0;

        std::array<VirtPageNumber, 5> vpns;
        do {
            inum--;

            std::size_t current_size;
            PackedVirtInstruction& current = instructions.read_instruction(current_size);
            Annotation& ann = output.start_write<Annotation>();
            ann.header.num_pages = current.store_page_numbers(vpns.data(), page_shift);
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
            output.finish_write(ann.size());
            max_working_set_size = std::max(max_working_set_size, next_access.size());

            if ((current.header.flags & FlagOutputPageFirstUse) != 0) {
                next_access.erase(pg_num(current.header.output, page_shift));
            }
        } while (inum != 0);

        return max_working_set_size;
    }

    std::uint64_t annotate_program(std::string annotations, std::string program, PageShift page_shift) {
        util::BufferedFileWriter<true> output(annotations.c_str());
        return annotate_program(output, program, page_shift);
    }
}
