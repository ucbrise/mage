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
 * @file util/stats.hpp
 * @brief Utilities for statistics collection.
 */

#ifndef MAGE_UTIL_STATS_HPP_
#define MAGE_UTIL_STATS_HPP_

#include <cstdint>
#include <algorithm>
#include <iostream>
#include <string>

namespace mage::util {
    /**
     * @brief Records statistics (min, mean, max, sum, count) of events.
     *
     * This is typically used to measure the latency of a type of event, both
     * of individual events and in aggregate, to understand its performance
     * impact.
     */
    class StreamStats {
        friend std::ostream& operator <<(std::ostream& out, const StreamStats& s);

    public:
        /**
         * @brief Creates a StreamStats object with the label "<anonymous>"
         * that will not automatically print out the measured statistics when
         * its destructor is called.
         */
        StreamStats() : StreamStats("<anonymous>") {
        }

        /**
         * @brief Creates a StreamStats object.
         *
         * @param name The label for this statistics collector, used when
         * printing out the measured statistics.
         * @param print_stats_on_exit If true, the statistics will be printed
         * when the destructor for this StreamStats is called.
         */
        StreamStats(std::string name, bool print_stats_on_exit = false) : label(name), print_on_exit(print_stats_on_exit),
            stat_max(0), stat_sum(0), stat_min(0), stat_count(0) {
        }

        /**
         * @brief Prints out the measured statistics if print_stats_on_exit was
         * specified.
         */
        ~StreamStats() {
            if (this->print_on_exit) {
                std::cout << *this << std::endl;
            }
        }

        /**
         * @brief Set the label for this statistics collector, used when
         * printing out the measured statistics.
         *
         * @param label The label for this statistics collector.
         * @param print_stats_on_exit If true, the statistics will be printed
         * when the destructor for this StreamStats is called.
         */
        void set_label(const std::string& label, bool print_stats_on_exit = true) {
            this->label = label;
            this->print_on_exit = print_stats_on_exit;
        }

        /**
         * @brief Record an event.
         *
         * @param stat The value for the event (typically a latency
         * measurement), used to compute the min, mean, max, and sum.
         */
        void event(std::uint64_t stat) {
            if (this->stat_count == 0) {
                this->stat_max = stat;
                this->stat_sum = stat;
                this->stat_min = stat;
                this->stat_count = 1;
            } else {
                this->stat_max = std::max(this->stat_max, stat);
                this->stat_sum += stat;
                this->stat_min = std::min(this->stat_min, stat);
                this->stat_count++;
            }
        }

    private:
        std::uint64_t stat_max;
        std::uint64_t stat_sum;
        std::uint64_t stat_min;
        std::uint64_t stat_count;

        std::string label;
        bool print_on_exit;
    };

    /**
     * @brief Prints out the measured statistics for a StreamStats object.
     */
    inline std::ostream& operator <<(std::ostream& out, const StreamStats& s) {
        return out << s.label << ": ( min = " << s.stat_min << ", avg = " << (s.stat_count == 0 ? 0 : s.stat_sum / s.stat_count) << ", max = " << s.stat_max << ", count = " << s.stat_count << ", sum = " << s.stat_sum << " )";
    }
}

#endif
