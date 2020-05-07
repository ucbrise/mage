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

#include "memprog/annotator.hpp"
#include <cassert>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include "memprog/program.hpp"
#include "memprog/programfile.hpp"
#include "platform/memory.hpp"

namespace mage::memprog {
    // inline void range_from_addr(VirtPageRange& result, VirtAddr addr, BitWidth width, std::uint8_t page_shift) {
    //     result.start = addr >> page_shift;
    //     result.end = (addr + width - 1) >> page_shift;
    // }
    //
    // bool merge_range(VirtPageRange& r1, const VirtPageRange& r2) {
    //     if (r2.start > r1.end || r1.start > r2.end) {
    //         return false;
    //     }
    //     r1.start = std::min(r1.start, r2.start);
    //     r1.end = std::max(r1.end, r2.end);
    //     return true;
    // }
    //
    // /* Stores the input ranges into the RANGES vector. */
    // void calculate_input_ranges(std::vector<VirtPageRange>& ranges, const VirtualInstruction& v, std::uint8_t page_shift) {
    //     if (v.input1 == invalid_vaddr) {
    //         ranges.resize(0);
    //         return;
    //     }
    //     ranges.resize(1);
    //     range_from_addr(ranges[0], v.input1, v.width, page_shift);
    //     if (v.input2 == invalid_vaddr) {
    //         return;
    //     }
    //     ranges.resize(2);
    //     range_from_addr(ranges[1], v.input2, v.width, page_shift);
    //     bool merged = merge_range(ranges[0], ranges[1]);
    //     if (v.input3 == invalid_vaddr) {
    //         if (merged) {
    //             ranges.resize(1);
    //         }
    //         return;
    //     }
    //     if (merged) {
    //         range_from_addr(ranges[1], v.input3, v.width, page_shift);
    //         if (merge_range(ranges[0], ranges[1])) {
    //             ranges.resize(1);
    //         } else {
    //             ranges.resize(2);
    //         }
    //     } else {
    //         ranges.resize(3);
    //         range_from_addr(ranges[2], v.input3, v.width, page_shift);
    //         int size;
    //         if (merge_range(ranges[0], ranges[2])) {
    //             if (merge_range(ranges[0], ranges[1])) {
    //                 ranges.resize(1);
    //             } else {
    //                 ranges.resize(2);
    //             }
    //         } else if (merge_range(ranges[1], ranges[2])) {
    //             ranges.resize(2);
    //         }
    //     }
    // }
    //
    // InstructionNumber annotate_input(VirtAddr input, InstructionNumber this_use, std::unordered_map<VirtPageNumber, InstructionNumber>& next_access, std::uint8_t page_shift) {
    //     if (input == invalid_vaddr) {
    //         return invalid_instr;
    //     }
    //     VirtPageNumber page = input >> page_shift;
    //     auto lookup_result = next_access.insert(std::make_pair(page, this_use));
    //     if (lookup_result.second) {
    //         return invalid_instr;
    //     } else {
    //         auto iter = lookup_result.first;
    //         InstructionNumber next_use = iter->second;
    //         iter->second = this_use;
    //         return next_use;
    //     }
    // }
    //
    // std::uint64_t reverse_annotate_program(std::string reverse_annotated_program, std::string original_program, std::uint8_t page_shift) {
    //     std::uint64_t max_working_set_size = 0;
    //
    //     platform::MappedFile<ProgramFileHeader> old_mapping(original_program.c_str());
    //     const ProgramFileHeader* original = old_mapping.mapping();
    //
    //     std::ofstream output;
    //     output.exceptions(std::ios::failbit | std::ios::badbit);
    //     output.open(reverse_annotated_program, std::ios::out | std::ios::binary | std::ios::trunc);
    //
    //     std::vector<VirtPageRange> input_ranges;
    //
    //     std::unordered_map<VirtPageNumber, InstructionNumber> next_access;
    //     for (InstructionNumber i = original->num_instructions - 1; i != UINT64_MAX; i--) {
    //         const VirtualInstruction& v = original->instructions[i];
    //
    //         VirtPageRange output_range;
    //         range_from_addr(output_range, v.output, v.width, page_shift);
    //
    //         InstructionAnnotationHeader header;
    //         header.magic = annotation_header_magic;
    //         calculate_input_ranges(input_ranges, v, page_shift);
    //         header.num_input_pages = 0;
    //         for (int j = 0; j != input_ranges.size(); j++) {
    //             VirtPageRange& r = input_ranges[j];
    //             header.num_input_pages += (r.end - r.start + 1);
    //         }
    //         header.num_output_pages = output_range.end - output_range.start + 1;
    //         output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    //
    //         for (int j = 0; j != input_ranges.size(); j++) {
    //             VirtPageRange& r = input_ranges[j];
    //             for (VirtPageNumber page = r.start; page != r.end + 1; page++) {
    //                 auto lookup_result = next_access.insert(std::make_pair(page, i));
    //                 if (lookup_result.second) {
    //                     output.write(reinterpret_cast<const char*>(&invalid_instr), sizeof(invalid_instr));
    //                 } else {
    //                     auto iter = lookup_result.first;
    //                     InstructionNumber next_use = iter->second;
    //                     iter->second = i;
    //                     output.write(reinterpret_cast<const char*>(&next_use), sizeof(next_use));
    //                 }
    //             }
    //         }
    //
    //         max_working_set_size = std::max(max_working_set_size, next_access.size());
    //
    //         for (VirtPageNumber page = output_range.end; page != output_range.start - 1; page--) {
    //             auto iter = next_access.find(page);
    //             if (iter == next_access.end()) {
    //                 // If this is not an output of the program, then this is dead.
    //                 output.write(reinterpret_cast<const char*>(&invalid_instr), sizeof(invalid_instr));
    //             } else {
    //                 InstructionNumber next_output_page_use = iter->second;
    //                 next_access.erase(page + 1);
    //                 output.write(reinterpret_cast<const char*>(&next_output_page_use), sizeof(next_output_page_use));
    //             }
    //         }
    //     }
    //
    //     return max_working_set_size;
    // }
    //
    // void unreverse_annotations(std::string annotated_program, std::string reverse_annotated_program) {
    //     platform::MappedFile<std::uint64_t> rev(reverse_annotated_program.c_str());
    //     platform::MappedFile<std::uint64_t> ann(annotated_program.c_str(), rev.size());
    //
    //     std::uint64_t* reversed = rev.mapping();
    //     std::uint64_t* annotations = ann.mapping();
    //     std::uint64_t* rev_end = reversed + rev.size() / sizeof(std::uint64_t);
    //     std::uint64_t* ann_cur = annotations + ann.size() / sizeof(std::uint64_t);
    //
    //     std::uint64_t* rev_cur = reversed;
    //     while (rev_cur != rev_end) {
    //         InstructionAnnotationHeader& header = *reinterpret_cast<InstructionAnnotationHeader*>(rev_cur);
    //         assert(header.magic == annotation_header_magic);
    //         std::uint64_t size = header.num_input_pages + header.num_output_pages + 1;
    //
    //         std::uint64_t* new_rev_cur = rev_cur + size;
    //         ann_cur = std::copy_backward(rev_cur, new_rev_cur, ann_cur);
    //         rev_cur = new_rev_cur;
    //     }
    // }
}
