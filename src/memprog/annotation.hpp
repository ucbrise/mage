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

#ifndef MAGE_MEMPROG_ANNOTATION_HPP_
#define MAGE_MEMPROG_ANNOTATION_HPP_

#include <cstdint>
#include <string>
#include "memprog/program.hpp"
#include "util/filebuffer.hpp"
#include "util/mapping.hpp"

namespace mage::memprog {
    struct Annotation {
        struct {
            std::uint16_t num_pages;
        } __attribute__((packed)) header;
        struct {
            InstructionNumber next_use : instruction_number_bits;
        } __attribute__((packed)) slots[5];

        std::uint16_t size() const {
            return sizeof(Annotation::header) + this->header.num_pages * sizeof(Annotation::slots[0]);
        }

        Annotation* next() {
            std::uint8_t* self = reinterpret_cast<std::uint8_t*>(this);
            return reinterpret_cast<Annotation*>(self + this->size());
        }

        const Annotation* next() const {
            const std::uint8_t* self = reinterpret_cast<const std::uint8_t*>(this);
            return reinterpret_cast<const Annotation*>(self + this->size());
        }
    } __attribute__((packed));

    std::uint64_t reverse_annotate_program(util::BufferedFileWriter& output, std::string program, PageShift page_shift);
    std::uint64_t reverse_annotate_program(std::string reverse_annotations, std::string program, PageShift page_shift);
    inline void unreverse_annotations(std::string annotations, std::string reverse_annotations) {
        util::reverse_file_into<Annotation>(reverse_annotations.c_str(), annotations.c_str());
    }
    std::uint64_t annotate_program(std::string annotations, std::string program, PageShift page_shift);
}

#endif
