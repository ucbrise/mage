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

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "util/binaryfile.hpp"

static inline std::uint64_t get_cyclic_party(std::uint64_t i, std::uint64_t num_workers, std::uint64_t total) {
    return i % num_workers;
}

static inline std::uint64_t get_blocked_party(std::uint64_t i, std::uint64_t num_workers, std::uint64_t total) {
    std::uint64_t per_party = total / num_workers;
    std::uint64_t extras = total % num_workers;
    std::uint64_t boundary = extras * (per_party + 1);
    if (i < boundary) {
        return i / (per_party + 1);
    } else {
        return extras + (i - boundary) / per_party;
    }
}

void write_record(mage::util::BinaryFileWriter* to, std::uint32_t key, std::uint32_t data1 = 0, std::uint32_t data2 = 0, std::uint32_t data3 = 0) {
    to->write32(key);
    to->write32(data1);
    to->write32(data2);
    to->write32(data3);
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cout << "Usage: " << argv[0] << " problem_name problem_size num_workers" << std::endl;
        return 0;
    }
    std::string problem_name(argv[1]);
    int input_size = std::stoi(std::string(argv[2]));
    int num_workers = std::stoi(std::string(argv[3]));

    std::vector<std::unique_ptr<mage::util::BinaryFileWriter>> garbler_writers(num_workers);
    std::vector<std::unique_ptr<mage::util::BinaryFileWriter>> evaluator_writers(num_workers);
    std::vector<std::unique_ptr<mage::util::BinaryFileWriter>> expected_writers(num_workers);

    for (int i = 0; i != num_workers; i++) {
        std::string common_prefix = problem_name + "_" + std::to_string(input_size) + "_" + std::to_string(i);

        garbler_writers[i] = std::make_unique<mage::util::BinaryFileWriter>((common_prefix + "_garbler.input").c_str());
        evaluator_writers[i] = std::make_unique<mage::util::BinaryFileWriter>((common_prefix + "_evaluator.input").c_str());
        expected_writers[i] = std::make_unique<mage::util::BinaryFileWriter>((common_prefix + ".expected").c_str());
    }

    if (problem_name == "aspirin" || problem_name == "aspirin_seq") {
        for (std::uint64_t i = 0; i != input_size * 2; i++) {
            std::uint64_t party = (i % num_workers);
            if (i < input_size) {
                garbler_writers[party]->write64((i << 32) | 1);
                garbler_writers[party]->write1(i == 0 ? 0 : 1);
                /* All patients except patient 0 have a diagnosis at t = 1. */
            } else {
                evaluator_writers[party]->write64(((2 * input_size - i - 1) << 32) | 2);
                evaluator_writers[party]->write1(0);
                /* All patients were prescribed aspirin at t = 2. */
            }
        }

        /* Correct output is 1, (input_size - 1). */
        expected_writers[0]->write1(1);
        expected_writers[0]->write32(input_size - 1);
    } else if (problem_name == "merge_sorted") {
        for (std::uint64_t i = 0; i != input_size * 2; i++) {
            std::uint64_t cyclic_party = get_cyclic_party(i, num_workers, input_size * 2);
            std::uint64_t blocked_party = get_blocked_party(i, num_workers, input_size * 2);
            if (i < input_size) {
                write_record(garbler_writers[cyclic_party].get(), 2 * i);
            } else {
                write_record(evaluator_writers[cyclic_party].get(), 2 * (2 * input_size - i - 1) + 1);
            }
            write_record(expected_writers[blocked_party].get(), i);
        }
    } else {
        std::cerr << "Unknown problem " << problem_name << std::endl;
    }

    return 0;
}
