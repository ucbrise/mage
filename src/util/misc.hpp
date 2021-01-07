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
 * @file util/misc.hpp
 * @brief Miscellaneous utility functions.
 */

#ifndef MAGE_UTIL_MISC_HPP_
#define MAGE_UTIL_MISC_HPP_

#include <cstdint>
#include <iostream>
#include <utility>

namespace mage::util {
    /**
     * @brief Determines if the specified positive number is a power of two.
     *
     * @tparam Type of the specified number.
     * @param number The specified number. It must be positive.
     * @return True if the specified number is a power of two, otherwise false.
     */
    template <typename T>
    bool is_power_of_two(T number) {
        return (number & (number - 1)) == 0;
    }

    /**
     * @brief Counts the number of 1s in the binary representation of the
     * specified unsigned number.
     *
     * This quantity is called the "Hamming Weight" of the specified unsigned
     * number.
     *
     * @param number The number in question.
     * @return The number of 1s in the binary representation of the specified
     * number.
     */
    static inline std::uint8_t hamming_weight(std::uint64_t number) {
        std::uint8_t weight = 0;
        while (number != 0) {
            weight += (number & 0x1);
            number >>= 1;
        }
        return weight;
    }

    /**
     * @brief Checks if the number of 1s in the binary representation of the
     * specified unsigned number is even or odd.
     *
     * @param number The number in question.
     * @return True if the number of 1s in the binary representation of the
     * specified number is odd, otherwise false.
     */
    static inline bool hamming_parity(std::uint64_t number) {
        return (hamming_weight(number) & 0x1) != 0x0;
    }

    /**
     * @brief Computes the base-2 logarithm of the specified number.
     *
     * @param The number whose logarithm to compute.
     * @return The smallest nonnegative number x such that (2 ^ x) >= number.
     */
    static inline std::uint8_t log_base_2(std::uint64_t number) {
        std::uint8_t logarithm = 0;
        while ((UINT64_C(1) << logarithm) < number) {
            logarithm++;
        }
        return logarithm;
    }

    /**
     * @brief Computes the floor division of two signed numbers.
     *
     * This function finds the unique quotient q and remainder r, where
     * 0 <= r < divisor, such that dividend = (q * divisor) + r.
     *
     * @param dividend The number to divide (i.e., the numerator).
     * @param divisor The number to divide by (i.e., the denominator).
     * @return A pair whose first item is the quotient q and whose second item
     * is the remainder r.
     */
    static inline std::pair<std::int64_t, std::int64_t> floor_div(std::int64_t dividend, std::int64_t divisor) {
        int64_t quotient = dividend / divisor;
        int64_t remainder = dividend % divisor;
        if (remainder < 0) {
            quotient -= 1;
            remainder += divisor;
        }
        return std::make_pair(quotient, remainder);
    }

    /**
     * @brief Computes the ceiling division of two signed numbers.
     *
     * This function finds the unique quotient q and remainder r, where
     * -divisor < r <= 0, such that dividend = (q * divisor) + r.
     *
     * @param dividend The number to divide (i.e., the numerator).
     * @param divisor The number to divide by (i.e., the denominator).
     * @return A pair whose first item is the quotient q and whose second item
     * is the remainder r.
     */
    static inline std::pair<std::int64_t, std::int64_t> ceil_div(std::int64_t dividend, std::int64_t divisor) {
        int64_t quotient = dividend / divisor;
        int64_t remainder = dividend % divisor;
        if (remainder > 0) {
            quotient += 1;
            remainder -= divisor;
        }
        return std::make_pair(quotient, remainder);
    }

    /**
     * @brief A specialization of std::streambuf supporting reads and writes
     * to an in-memory buffer.
     */
    class MemoryBuffer : public std::streambuf {
        /**
         * @brief Creates a std::streambuf for reading and writing to an
         * in-memory buffer.
         *
         * @param buffer A pointer to the in-memory buffer to use.
         * @param length The size, in bytes, of the in-memory buffer.
         */
        MemoryBuffer(void* buffer, std::size_t length) {
            char* base = static_cast<char*>(buffer);
            this->setg(base, base, base + length);
            this->setp(base, base + length);
        }
    };

    /**
     * @brief A specialization of std::streambuf supporting reads from an
     * in-memory buffer.
     */
    class MemoryReadBuffer : public std::streambuf {
    public:
        /**
         * @brief Creates a std::streambuf for reading from an in-memory
         * buffer.
         *
         * @param buffer A pointer to the in-memory buffer to use.
         * @param length The size, in bytes, of the in-memory buffer.
         */
        MemoryReadBuffer(const void* buffer, std::size_t length) {
            char* base = const_cast<char*>(static_cast<const char*>(buffer));
            this->setg(base, base, base + length);
        }
    };

    /**
     * @brief A specialization of std::streambuf supporting writes to an
     * in-memory buffer.
     */
    class MemoryWriteBuffer : public std::streambuf {
    public:
        /**
         * @brief Creates a std::streambuf for writing to an in-memory buffer.
         *
         * @param buffer A pointer to the in-memory buffer to use.
         * @param length The size, in bytes, of the in-memory buffer.
         */
        MemoryWriteBuffer(void* buffer, std::size_t length) {
            char* base = static_cast<char*>(buffer);
            this->setp(base, base + length);
        }
    };
}

#endif
