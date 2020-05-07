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

#include "memprog/program.hpp"
#include <cassert>
#include <memory>

namespace mage::memprog {
    Program* Program::current_working_program = nullptr;

    Program::Program(PageShift shift) : next_free_address(0), page_shift(shift) {
    }

    Program::~Program() {
        if (Program::current_working_program == this) {
            Program::current_working_program = nullptr;
        }
    }

    Program* Program::set_current_working_program(Program* cwp) {
        Program* old_cwp = Program::current_working_program;
        Program::current_working_program = cwp;
        return old_cwp;
    }

    Program* Program::get_current_working_program() {
        return Program::current_working_program;
    }
}
