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

#ifndef MAGE_MEMPROG_SCHEDULING_HPP_
#define MAGE_MEMPROG_SCHEDULING_HPP_

#include <cstdlib>
#include <unordered_map>
#include "addr.hpp"
#include "instruction.hpp"
#include "programfile.hpp"

namespace mage::memprog {
    class Scheduler {
    public:
        Scheduler(std::string input_file, std::string output_file);
        virtual ~Scheduler();

        virtual void schedule() = 0;

    protected:
        void emit_finish_swapin(PhysPageNumber ppn);
        void emit_finish_swapout(PhysPageNumber ppn);

        PhysProgramFileReader input;
        PhysProgramFileWriter output;
    };

    class NOPScheduler : public Scheduler {
    public:
        NOPScheduler(std::string input_file, std::string output_file);

        void schedule() override;
    };

    class BackdatingScheduler {
    public:
        BackdatingScheduler(std::string input_file, std::string output_file, std::uint64_t max_in_flight);

    private:
        std::unordered_map<PhysPageNumber, PhysPageNumber> memory_relocation;
        std::unordered_map<StoragePageNumber, StoragePageNumber> storage_relocation;
    };
}

#endif
