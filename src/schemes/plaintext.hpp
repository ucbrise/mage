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

#ifndef MAGE_SCHEMES_PLAINTEXT_HPP_
#define MAGE_SCHEMES_PLAINTEXT_HPP_

namespace mage::schemes {
    class Plaintext {
    public:
        using Wire = unsigned __int128;

        void op_and(Wire& output, const Wire& input1, const Wire& input2) {
            output = input1 & input2;
        }

        void op_xor(Wire& output, const Wire& input1, const Wire& input2) {
            output = input1 ^ input2;
        }

        void op_not(Wire& output, const Wire& input) {
            output = !input;
        }

        void op_xnor(Wire& output, const Wire& input1, const Wire& input2) {
            output = !(input1 ^ input2);
        }

        void op_copy(Wire& output, const Wire& input) {
            output = input;
        }

        void one(Wire& output) const {
            output = 1;
        }

        void zero(Wire& output) const {
            output = 0;
        }
    };
}

#endif
