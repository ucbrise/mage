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
#include <algorithm>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include "util/binaryfile.hpp"
#include "util/misc.hpp"

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
    if (argc != 4 && argc != 5) {
        std::cout << "Usage: " << argv[0] << " problem_name problem_size num_workers [option]" << std::endl;
        return 0;
    }
    std::string problem_name(argv[1]);
    int input_size = std::stoi(std::string(argv[2]));
    int num_workers = std::stoi(std::string(argv[3]));

    std::string option;
    if (argc == 5) {
        option = argv[4];
    }

    std::vector<std::unique_ptr<mage::util::BinaryFileWriter>> garbler_writers(num_workers);
    std::vector<std::unique_ptr<mage::util::BinaryFileWriter>> evaluator_writers(num_workers);
    std::vector<std::unique_ptr<mage::util::BinaryFileWriter>> expected_writers(num_workers);

    if (option != "check") {
        for (int i = 0; i != num_workers; i++) {
            std::string common_prefix = problem_name + "_" + std::to_string(input_size) + "_" + std::to_string(i);

            garbler_writers[i] = std::make_unique<mage::util::BinaryFileWriter>((common_prefix + "_garbler.input").c_str());
            evaluator_writers[i] = std::make_unique<mage::util::BinaryFileWriter>((common_prefix + "_evaluator.input").c_str());
            expected_writers[i] = std::make_unique<mage::util::BinaryFileWriter>((common_prefix + ".expected").c_str());
        }
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
    } else if (problem_name == "full_sort") {
        if (option == "") {
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
        } else if (option == "random") {
            std::vector<std::uint32_t> sorted(2 * input_size);
            for (std::uint64_t i = 0; i != sorted.size(); i++) {
                sorted[i] = static_cast<std::uint32_t>(i);
            }
            std::vector<std::uint32_t> array(sorted);
            std::random_shuffle(array.begin(), array.end());
            for (std::uint64_t i = 0; i != input_size * 2; i++) {
                std::uint64_t cyclic_party = get_cyclic_party(i, num_workers, input_size * 2);
                std::uint64_t blocked_party = get_blocked_party(i, num_workers, input_size * 2);
                if (i < input_size) {
                    write_record(garbler_writers[cyclic_party].get(), array[i]);
                } else {
                    write_record(evaluator_writers[cyclic_party].get(), array[i]);
                }
                write_record(expected_writers[blocked_party].get(), sorted[i]);
            }
        } else {
            std::cerr << "Unkown option " << option << std::endl;
        }
    } else if (problem_name == "loop_join") {
        std::vector<std::uint32_t> table1_keys(input_size);
        std::iota(table1_keys.begin(), table1_keys.end(), 0);
        std::vector<std::uint32_t> table2_keys(input_size);
        std::iota(table2_keys.begin(), table2_keys.end(), 0);
        if (option == "") {
            for (std::uint64_t i = 0; i != table1_keys.size() + table2_keys.size(); i++) {
                std::uint64_t cyclic_party = get_cyclic_party(i, num_workers, table1_keys.size() + table2_keys.size());
                if (i < table1_keys.size()) {
                    write_record(garbler_writers[cyclic_party].get(), table1_keys[i]);
                } else {
                    write_record(evaluator_writers[cyclic_party].get(), table2_keys[i - table1_keys.size()]);
                }
            }

            std::size_t join_size = table1_keys.size() * table2_keys.size();
            for (std::uint64_t i = 0, k = 0; i != table1_keys.size(); i++) {
                for (std::uint64_t j = 0; j != table2_keys.size(); j++, k++) {
                    std::uint64_t blocked_party = get_blocked_party(k, num_workers, join_size);
                    bool valid = (table1_keys[i] < table2_keys[j]);
                    if (valid) {
                        expected_writers[blocked_party]->write1(1);
                        write_record(expected_writers[blocked_party].get(), table1_keys[i]);
                        write_record(expected_writers[blocked_party].get(), table2_keys[j]);
                    } else {
                        expected_writers[blocked_party]->write1(0);
                        write_record(expected_writers[blocked_party].get(), 0);
                        write_record(expected_writers[blocked_party].get(), 0);
                    }
                }
            }
        } else if (option == "check") {
            std::size_t join_size = table1_keys.size() * table2_keys.size();
            std::vector<std::pair<std::uint32_t, uint32_t>> expected;
            for (std::uint64_t i = 0, k = 0; i != table1_keys.size(); i++) {
                for (std::uint64_t j = 0; j != table2_keys.size(); j++, k++) {
                    std::uint64_t blocked_party = get_blocked_party(k, num_workers, join_size);
                    bool valid = (table1_keys[i] < table2_keys[j]);
                    if (valid) {
                        expected.push_back(std::make_pair(table1_keys[i], table2_keys[j]));
                    }
                }
            }

            bool fail = false;
            std::vector<std::pair<std::uint32_t, uint32_t>> actual;
            std::uint64_t actual_total = 0;
            std::uint64_t expected_records_per_worker = join_size / num_workers;
            std::uint64_t expected_bits_per_worker = expected_records_per_worker * 257;
            std::uint64_t expected_bytes_per_worker = mage::util::ceil_div(expected_bits_per_worker, 8).first;
            for (int w = 0; w != num_workers; w++) {
                std::string output_file_name(problem_name + "_" + std::to_string(input_size) + "_" + std::to_string(w) + ".output");
                mage::util::BinaryFileReader r(output_file_name.c_str());
                if (r.get_file_length() != expected_bytes_per_worker) {
                    std::cerr << "Expected " << expected_bytes_per_worker << " bytes in " << output_file_name << ", but found " << r.get_file_length() << " bytes" << std::endl;
                    fail = true;
                    continue;
                }
                for (std::uint64_t i = 0; i != expected_records_per_worker; i++) {
                    std::uint8_t valid = r.read1();
                    std::uint32_t t1_record[4];
                    std::uint32_t t2_record[4];
                    r.read_bytes(reinterpret_cast<std::uint8_t*>(&t1_record[0]), 16);
                    r.read_bytes(reinterpret_cast<std::uint8_t*>(&t2_record[0]), 16);
                    if (valid == 1) {
                        actual.push_back(std::make_pair(t1_record[0], t2_record[0]));
                    }
                }
            }

            if (!fail) {
                if (actual.size() == expected.size()) {
                    std::sort(actual.begin(), actual.end());
                    for (std::uint64_t i = 0; i != actual.size(); i++) {
                        if (actual[i] != expected[i]) {
                            std::cerr << "Actual and expected differ at position " << i << " in the sorted result" << std::endl;
                            fail = true;
                        }
                    }
                } else {
                    std::cerr << "Expected " << expected.size() << " items in the join, but only found " << actual.size() << " items" << std::endl;
                    fail = true;
                }
            }
            if (!fail) {
                std::cout << "PASS" << std::endl;
            }
        }
    } else {
        std::cerr << "Unknown problem " << problem_name << std::endl;
    }

    return 0;
}