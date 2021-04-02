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

#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include "platform/misc.hpp"
#include "util/misc.hpp"
#include "util/progress.hpp"

namespace mage::util {
    ProgressBar::ProgressBar(const std::string& label, std::uint64_t total_units) {
        this->reset(label, total_units);
    }

    void ProgressBar::set_label(const std::string& label) {
        this->display_name = label;
        this->current_width = 0;
    }

    void ProgressBar::reset(const std::string& label, std::uint64_t total_units) {
        this->set_label(label);
        this->reset(total_units);
    }

    void ProgressBar::reset(std::uint64_t total_units) {
        this->update_threshold = total_units;
        this->next_update = 0;
        this->current_count = 0;
        this->total_count = total_units;
    }

    void ProgressBar::erase() const {
        if (this->current_width != 0) {
            std::cout << '\r';
        }
    }

    void ProgressBar::display() const {
        if (this->current_width != 0) {
            std::cout << this->bar << std::flush;
        }
    }

    void ProgressBar::finish(bool fill) {
        if (this->current_width != 0) {
            if (fill) {
                this->refresh(this->total_count);
            }
            std::cout << std::endl;
        }
    }

    void ProgressBar::update(std::uint64_t num_units) {
        std::uint32_t percentage = (100 * num_units) / this->total_count;
        char* percent_start = this->get_percent_start();
        int written = std::snprintf(percent_start, 4, "%3" PRIu32, percentage);
        percent_start[written] = '%';

        char* bar_start = this->get_bar_start();
        std::uint32_t bar_length = (this->bar_capacity * num_units) / this->total_count;
        bar_length = std::min(bar_length, this->bar_capacity);
        std::uint32_t i;
        for (i = 0; i != bar_length; i++) {
            bar_start[i] = ProgressBar::bar_full;
        }
        for (; i != this->bar_capacity; i++) {
            bar_start[i] = ProgressBar::bar_empty;
        }
    }

    bool ProgressBar::reconstruct_bar_if_necessary() {
        platform::TerminalSize ts;
        platform::get_terminal_size(ts);
        if (ts.num_cols != this->current_width) {
            this->current_width = ts.num_cols;
            this->construct_bar();
        }
        return this->current_width != 0;
    }

    void ProgressBar::construct_bar() {
        constexpr const char* preamble = ": [  0%] [";

        /* 1 is for \r at the beginning. */
        this->bar_start = 1 + this->display_name.length() + std::strlen(preamble);

        /* +1 for the ']' at the end, but -1 for the \r (which doesn't actually take up space.) */
        std::uint32_t bar_space = this->bar_start + 1 - 1;
        if (bar_space > this->current_width) {
            this->bar_capacity = 0;
        } else {
            this->bar_capacity = this->current_width - bar_space;
        }

        std::ostringstream buffer;
        buffer << '\r' << this->display_name << preamble;
        for (std::uint32_t i = 0; i != this->bar_capacity; i++) {
            buffer << ProgressBar::bar_empty;
        }
        buffer << ']';
        this->bar = buffer.str();

        this->update_threshold = this->total_count / std::max(this->bar_capacity, UINT32_C(100));
        if (this->update_threshold == 0) {
            this->update_threshold = 1;
        }
    }

    char* ProgressBar::get_bar_start() {
        return &this->bar[this->bar_start];
    }

    char* ProgressBar::get_percent_start() {
        return &this->bar[this->bar_start - 7];
    }
}
