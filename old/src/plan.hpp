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

#ifndef MAGE_PLAN_HPP_
#define MAGE_PLAN_HPP_

#include <cstdint>
#include <iostream>
#include <fstream>

#include "stream.hpp"

namespace mage {
    using WireMemoryLocation = std::uint64_t;
    using WireStorageLocation = std::uint64_t;

    enum class PlannedActionType : std::uint8_t {
        GateExecTable,
        GateExecXOR,
        GateExecXNOR,
        SwapOut,
        SwapIn
    };

    struct GateExecAction {
        WireMemoryLocation input1;
        WireMemoryLocation input2;
        WireMemoryLocation output;
    };

    struct SwapOutAction {
        WireMemoryLocation primary;
        WireStorageLocation secondary;
    };

    struct SwapInAction {
        WireStorageLocation secondary;
        WireMemoryLocation primary;
    };

    struct PlannedAction {
        PlannedActionType opcode;
        union {
            GateExecAction exec;
            SwapOutAction swapout;
            SwapInAction swapin;
        };
    };

    struct Plan {
        std::uint64_t num_actions;
        std::uint64_t num_wire_slots;
        PlannedAction actions[0];
    };

    using PlanWriter = StreamWriter<PlannedAction>;

    class FilePlanWriter : public FileStreamWriter<PlannedAction> {
    public:
        FilePlanWriter(std::string filename, std::uint64_t num_wire_slots);
        ~FilePlanWriter();
        void append(const PlannedAction& action) override;

    private:
        std::uint64_t num_actions;
        std::ofstream output;
    };
}

#endif
