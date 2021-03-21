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

/**
 * @file memprog/annotation.hpp
 * @brief Annotation reverse pass for MAGE's planner
 *
 * The annotation reverse pass is needed to apply Belady's theoretically
 * optimal paging algorithm (MIN) in the Replacement phase.
 */

#ifndef MAGE_MEMPROG_ANNOTATION_HPP_
#define MAGE_MEMPROG_ANNOTATION_HPP_

#include <cstdint>
#include <string>
#include "memprog/program.hpp"
#include "util/filebuffer.hpp"

namespace mage::memprog {
    /**
     * @brief Structure describing the encoding of annotations.
     *
     * An annotation describes, for page accessed by an instruction, the
     * position of the next instruction that acesses that page, or
     * @p invalid_instr if no future instruction accesses that page.
     */
    struct Annotation {
        struct {
            std::uint16_t num_pages;
        } __attribute__((packed)) header;
        struct {
            InstructionNumber next_use : instruction_number_bits;
        } __attribute__((packed)) slots[5];

        /**
         * @brief Computes the size of this annotation based on its header.
         *
         * This is useful when reading annotations from a file.
         *
         * @return The size of this annotation.
         */
        std::uint16_t size() const {
            return sizeof(Annotation::header) + this->header.num_pages * sizeof(Annotation::slots[0]);
        }

        /**
         * @brief Computes the address of the next annotation in the sequence,
         * assuming that annotations are packed together sequentially in
         * memory.
         *
         * This is particularly useful when a file containing annotations is
         * mapped into memory.
         *
         * @return The address of the next annotation in the sequence.
         */
        Annotation* next() {
            std::uint8_t* self = reinterpret_cast<std::uint8_t*>(this);
            return reinterpret_cast<Annotation*>(self + this->size());
        }

        /**
         * @brief Computes the address of the next annotation in the sequence,
         * assuming that annotations are packed together sequentially in
         * memory.
         *
         * This is particularly useful when a file containing annotations is
         * mapped into memory.
         *
         * @return The address of the next annotation in the sequence.
         */
        const Annotation* next() const {
            const std::uint8_t* self = reinterpret_cast<const std::uint8_t*>(this);
            return reinterpret_cast<const Annotation*>(self + this->size());
        }
    } __attribute__((packed));

    /**
     * @brief Computes annotations for a virtual bytecode.
     *
     * This involves iterating over the virtual bytecode in reverse order.
     *
     * @param annotations The file name to which the annotations should be
     * written.
     * @param program The file name containing the virtual bytecode to read.
     * This sequence of instructions should be reverse-iterable (e.g., written
     * with a BufferedFileWriter with backwards_readable == true).
     * @param page_shift Base-2 logarithm of the page size.
     * @param progress_bar Progress bar to use to show progress, or nullptr if
     * none should be used.
     */
    std::uint64_t annotate_program(std::string annotations, std::string program, PageShift page_shift, util::ProgressBar* progress_bar = nullptr);
}

#endif
