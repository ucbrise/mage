/*
 * Copyright (C) 2021 Sam Kumar <samkumar@cs.berkeley.edu>
 * Copyright (C) 2021 University of California, Berkeley
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
 * @file util/progress.hpp
 * @brief Terminal-based ASCII progress bar.
 */

#ifndef MAGE_UTIL_PROGRESS_HPP_
#define MAGE_UTIL_PROGRESS_HPP_

#include <cstdint>
#include <string>
#include "util/misc.hpp"

namespace mage::util {
    /**
     * @brief Represents a ASCII terminal-based progress bar.
     *
     * The width of the bar is determined automatically based on the width of
     * the terminal corresponding to standard output; if standard output does
     * not correspond to a terminal, then no progress bar is displayed.
     */
    class ProgressBar {
    public:
        /**
         * @brief Creates a progress bar with the specified label and units of
         * work.
         *
         * Initially, zero units of work are completed for the progress bar;
         * it will be printed out (displayed) on the first call to @p refresh.
         *
         * @param label A string with the label to print out to the left of
         * the progress bar.
         * @param total_units The total number of units of work in the task
         * whose progress is being measured.
         */
        ProgressBar(const std::string& label = "", std::uint64_t total_units = 0);

        /**
         * @brief Sets the label on a progress bar.
         *
         * @param label A string with the label to print out to the left of
         * the progress bar.
         */
        void set_label(const std::string& label);

        /**
         * @brief Re-initializes a progress bar, resetting it to the state of a
         * newly constructed progress bar.
         *
         * @param label A string with the label to print out to the left of
         * the progress bar.
         * @param total_units The total number of units of work in the task
         * whose progress is being measured.
         */
        void reset(const std::string& label, std::uint64_t total_units);

        /**
         * @brief Re-initializes a progress bar, resetting it to the state of a
         * newly constructed progress bar. Retains the label from before.
         *
         * @param total_units The total number of units of work in the task
         * whose progress is being measured.
         */
        void reset(std::uint64_t total_units);

        /**
         * @brief Erases the on-screen progress bar, replacing it with an
         * empty row in the terminal.
         */
        void erase() const;

        /**
         * @brief Displays the progress bar, causing it to re-appear after a
         * previous call to @p erase().
         */
        void display() const;

        /**
         * @brief Advances the terminal to the next line, leaving the progress
         * bar visible on the previous row.
         *
         * @param fill If true, the progress bar is updated to a "full" state
         * (task 100% completed) before advancing to the next line.
         */
        void finish(bool fill = true);

        /**
         * @brief Increases the progress displayed on the progress bar by the
         * specified amount.
         *
         * @param num_units The number of additional units of work completed
         * for the task whose progress is being measured.
         */
        void advance(std::uint64_t num_units) {
            this->refresh(this->current_count + num_units);
        }

        /**
         * @brief Increases the progress displayed on the progress bar to the
         * specified amount.
         *
         * @pre The argument @p num_units must be greater than or equal to the
         * value of @p num_units in previous calls to this function and less
         * than or equal to the value of @p total_units used to initialize this
         * progress bar, via the constructor or the @p reset function.
         * @param num_units The number of total units of work completed in the
         * task whose progress is being measured.
         */
        void refresh(std::uint64_t num_units) {
            this->current_count = num_units;
            if (num_units >= this->next_update) {
                if (this->reconstruct_bar_if_necessary()) {
                    this->update(num_units);
                    this->display();
                }
                this->next_update = static_cast<std::uint64_t>(util::ceil_div(num_units + 1, this->update_threshold).first) * this->update_threshold;
                if (this->next_update > this->total_count) {
                    this->next_update = this->total_count;
                }
            }
        }

    private:
        void update(std::uint64_t num_units);

        bool reconstruct_bar_if_necessary();
        void construct_bar();
        char* get_bar_start();
        char* get_percent_start();

        std::uint64_t update_threshold;
        std::uint64_t next_update;
        std::uint64_t current_count;
        std::uint64_t total_count;
        std::uint32_t bar_start;
        std::uint32_t bar_capacity;
        std::uint32_t current_width;
        std::string bar;
        std::string display_name;

        static constexpr const char bar_full = '#';
        static constexpr const char bar_empty = '.';
    };
}

#endif
