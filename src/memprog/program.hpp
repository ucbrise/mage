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

#ifndef MAGE_MEMPROG_PROGRAM_HPP_
#define MAGE_MEMPROG_PROGRAM_HPP_

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include "addr.hpp"
#include "instruction.hpp"
#include "opcode.hpp"
#include "programfile.hpp"
#include "stream.hpp"

namespace mage::memprog {
    class Program : public VirtProgramFileWriter {
    public:
        Program(std::string filename, PageShift shift = 16);
        ~Program();

        Instruction& instruction() {
            return this->current;
        }

        VirtAddr commit_instruction(BitWidth output_width) {
            if (output_width != 0) {
                bool fresh_page;
                this->current.header.output = this->allocate_virtual(output_width, fresh_page);
                if (fresh_page) {
                    this->current.header.flags |= FlagOutputPageFirstUse;
                }
            }
            this->append_instruction(this->current);
            return this->current.header.output;
        }

        static Program* set_current_working_program(Program* cwp);
        static Program* get_current_working_program();

    private:
        VirtAddr allocate_virtual(BitWidth width, bool& fresh_page) {
            VirtAddr addr;
            assert(width != 0);
            if (pg_num(this->next_free_address, this->page_shift) == pg_num(this->next_free_address + width - 1, this->page_shift)) {
                addr = this->next_free_address;
            } else {
                addr = pg_next(this->next_free_address, this->page_shift);
            }
            this->next_free_address = addr + width;
            fresh_page = (pg_offset(addr, this->page_shift) == 0);
            return addr;
        }

        Instruction current;
        VirtAddr next_free_address;
        PageShift page_shift;
        static Program* current_working_program;
    };
}

#endif
