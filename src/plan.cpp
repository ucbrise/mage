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

#include "plan.hpp"

#include <cstdint>
#include <iostream>
#include <fstream>

namespace mage {
    FilePlanWriter::FilePlanWriter(std::string filename, std::uint64_t num_wire_slots)
        : FileStreamWriter<PlannedAction>::FileStreamWriter(filename), num_actions(0) {
        struct Plan plan;
        plan.num_actions = 0;
        plan.num_wire_slots = num_wire_slots;
        this->output.write(reinterpret_cast<const char*>(&plan), sizeof(plan));
    }

    FilePlanWriter::~FilePlanWriter() {
        this->output.seekp(0, std::ios::beg);
        this->output.write(reinterpret_cast<const char*>(&this->num_actions), sizeof(Plan::num_actions));
    }

    void FilePlanWriter::append(const PlannedAction& action) {
        this->FileStreamWriter<PlannedAction>::append(action);
        this->num_actions++;
    }
}
