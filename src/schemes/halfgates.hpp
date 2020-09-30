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

/*
 * The crypto logic is lifted from gc/halfgate_eva.h, gc/halfgate_gen.h, and
 * garble/garble_gates_halfgates.h in EMP-toolkit, but it has been modified to
 * work with the rest of MAGE.
 */

#ifndef MAGE_SCHEMES_HALFGATES_HPP_
#define MAGE_SCHEMES_HALFGATES_HPP_

#include <cstdint>
#include <string>
#include "crypto/aes.hpp"
#include "crypto/block.hpp"
#include "crypto/mitccrh.hpp"
#include "crypto/prg.hpp"
#include "crypto/prp.hpp"

namespace mage::schemes {
    class HalfGatesGarbler {
    public:
        using Wire = crypto::block;

        HalfGatesGarbler() : global_id(0) {
        }

        crypto::block initialize(Wire& delta_precursor) {
            crypto::PRG tmp;
            tmp.random_block(&delta_precursor);

            return this->initialize_with_delta(delta_precursor);
        }

        crypto::block initialize_with_delta(Wire& existing_delta_precursor) {
            this->set_delta(existing_delta_precursor);

            crypto::block input_seed;
            this->prg.random_block(&input_seed);
            this->shared_prg.set_seed(input_seed);

            return input_seed;
        }

        void input_garbler(Wire* data, unsigned int length, const bool* input_bits) {
            this->shared_prg.random_block(data, length);
            for (unsigned int i = 0; i != length; i++) {
                if (input_bits[i]) {
                    data[i] = crypto::xorBlocks(data[i], this->delta);
                }
            }
        }

        void input_evaluator(Wire* data, unsigned int length, std::pair<Wire, Wire>* ot_pairs) {
            this->prg.random_block(data, length);
            for (unsigned int i = 0; i != length; i++) {
                ot_pairs[i].first = data[i];
                ot_pairs[i].second = crypto::xorBlocks(data[i], this->delta);
                // std::cout << *reinterpret_cast<std::uint64_t*>(&pairs[i].first) << " OR " << *reinterpret_cast<std::uint64_t*>(&pairs[i].second) << std::endl;
            }
        }

        // HACK: assume all output goes to the garbler
        void output(std::uint8_t* into, const Wire* data, unsigned int length) {
            for (unsigned int i = 0; i != length; i++) {
                bool lsb = crypto::getLSB(data[i]);
                into[i] = lsb ? 0x1 : 0x0;
            }
        }

        void op_and(crypto::block* table, Wire& output, const Wire& input1, const Wire& input2) {
            crypto::block out1;
            garble_gate_garble_halfgates(input1, crypto::xorBlocks(input1, this->delta), input2, crypto::xorBlocks(input2, this->delta), &output, &out1, this->delta, table, this->global_id++, &this->prp.aes);
        }

        void op_xor(Wire& output, const Wire& input1, const Wire& input2) {
            output = crypto::xorBlocks(input1, input2);
        }

        void op_not(Wire& output, const Wire& input) {
            output = crypto::xorBlocks(input, this->public_constants[1]);
        }

        void op_xnor(Wire& output, const Wire& input1, const Wire& input2) {
            output = crypto::xorBlocks(crypto::xorBlocks(input1, input2), this->public_constants[1]);
        }

        void op_copy(Wire& output, const Wire& input) {
            output = input;
        }

        void one(Wire& output) const {
            output = this->public_constants[1];
        }

        void zero(Wire& output) const {
            output = this->public_constants[0];
        }

        crypto::block get_delta() const {
            return this->delta;
        }

    private:
        void set_delta(const Wire& x) {
            this->delta = crypto::make_delta(x);

            crypto::PRG tmp(crypto::fix_key);
            tmp.random_block(this->public_constants, 2);
            this->public_constants[1] = crypto::xorBlocks(this->public_constants[1], this->delta);
        }

        static inline void garble_gate_garble_halfgates(crypto::block LA0, crypto::block A1, crypto::block LB0, crypto::block B1, crypto::block *out0, crypto::block *out1, crypto::block delta, crypto::block *table, std::uint64_t idx, const crypto::AES_KEY* key) {
            long pa = crypto::getLSB(LA0);
            long pb = crypto::getLSB(LB0);
            crypto::block tweak1, tweak2;
            crypto::block HLA0, HA1, HLB0, HB1;
            crypto::block tmp, W0;

            tweak1 = crypto::makeBlock(2 * idx, (std::uint64_t) 0);
            tweak2 = crypto::makeBlock(2 * idx + 1, (std::uint64_t) 0);

            {
                crypto::block keys[4];
                crypto::block masks[4];

                keys[0] = crypto::xorBlocks(crypto::double_block(LA0), tweak1);
                keys[1] = crypto::xorBlocks(crypto::double_block(A1), tweak1);
                keys[2] = crypto::xorBlocks(crypto::double_block(LB0), tweak2);
                keys[3] = crypto::xorBlocks(crypto::double_block(B1), tweak2);
                masks[0] = keys[0];
                masks[1] = keys[1];
                masks[2] = keys[2];
                masks[3] = keys[3];
                AES_ecb_encrypt_blks(keys, 4, key);
                HLA0 = crypto::xorBlocks(keys[0], masks[0]);
                HA1 = crypto::xorBlocks(keys[1], masks[1]);
                HLB0 = crypto::xorBlocks(keys[2], masks[2]);
                HB1 = crypto::xorBlocks(keys[3], masks[3]);
            }

            table[0] = crypto::xorBlocks(HLA0, HA1);
            if (pb)
                table[0] = crypto::xorBlocks(table[0], delta);
            W0 = HLA0;
            if (pa)
                W0 = crypto::xorBlocks(W0, table[0]);
            tmp = crypto::xorBlocks(HLB0, HB1);
            table[1] = crypto::xorBlocks(tmp, LA0);
            W0 = crypto::xorBlocks(W0, HLB0);
            if (pb)
                W0 = crypto::xorBlocks(W0, tmp);

            *out0 = W0;
            *out1 = crypto::xorBlocks(*out0, delta);
        }

        static inline void garble_gate_garble_halfgates_mitccrh(crypto::block LA0, crypto::block A1, crypto::block LB0, crypto::block B1, crypto::block *out0, crypto::block *out1, crypto::block delta, crypto::block *table, crypto::MiTCCRH *mitccrh) {
            long pa = crypto::getLSB(LA0);
            long pb = crypto::getLSB(LB0);
            crypto::block HLA0, HA1, HLB0, HB1;
            crypto::block tmp, W0;

            crypto::block H[4];
            mitccrh->k2_h4(LA0, A1, LB0, B1, H);
            HLA0 = H[0];
            HA1 = H[1];
            HLB0 = H[2];
            HB1 = H[3];

            table[0] = crypto::xorBlocks(HLA0, HA1);
            if (pb)
                table[0] = crypto::xorBlocks(table[0], delta);
            W0 = HLA0;
            if (pa)
                W0 = crypto::xorBlocks(W0, table[0]);
            tmp = crypto::xorBlocks(HLB0, HB1);
            table[1] = crypto::xorBlocks(tmp, LA0);
            W0 = crypto::xorBlocks(W0, HLB0);
            if (pb)
                W0 = crypto::xorBlocks(W0, tmp);

            *out0 = W0;
            *out1 = crypto::xorBlocks(*out0, delta);
        }

        std::int64_t global_id;
        Wire delta;
        Wire public_constants[2];

        crypto::PRP prp;
        crypto::PRG prg;
        crypto::PRG shared_prg;
    };

    class HalfGatesEvaluator {
    public:
        using Wire = crypto::block;

        HalfGatesEvaluator() : global_id(0) {
        }

        void initialize(crypto::block input_seed) {
            crypto::PRG tmp(crypto::fix_key);
            tmp.random_block(this->public_constants, 2);

            this->shared_prg.set_seed(input_seed);
        }

        void input_garbler(Wire* data, unsigned int length) {
            this->shared_prg.random_block(data, length);
        }

        // HACK: assume all output goes to the garbler
        void output(std::uint8_t* into, const Wire* data, unsigned int length) {
            for (unsigned int i = 0; i != length; i++) {
                bool lsb = crypto::getLSB(data[i]);
                into[i] = lsb ? 0x1 : 0x0;
            }
        }

        void op_and(crypto::block* table, Wire& output, const Wire& input1, const Wire& input2) {
            garble_gate_eval_halfgates(input1, input2, &output, table, this->global_id++, &this->prp.aes);
        }

        void op_xor(Wire& output, const Wire& input1, const Wire& input2) {
            output = crypto::xorBlocks(input1, input2);
        }

        void op_not(Wire& output, const Wire& input) {
            output = crypto::xorBlocks(input, this->public_constants[1]);
        }

        void op_xnor(Wire& output, const Wire& input1, const Wire& input2) {
            output = crypto::xorBlocks(crypto::xorBlocks(input1, input2), this->public_constants[1]);
        }

        void op_copy(Wire& output, const Wire& input) {
            output = input;
        }

        void one(Wire& output) const {
            output = this->public_constants[1];
        }

        void zero(Wire& output) const {
            output = this->public_constants[0];
        }

    private:
        static inline void garble_gate_eval_halfgates(crypto::block A, crypto::block B, crypto::block *out, const crypto::block *table, std::uint64_t idx, const crypto::AES_KEY* key) {
            crypto::block HA, HB, W;
            int sa, sb;
            crypto::block tweak1, tweak2;

            sa = crypto::getLSB(A);
            sb = crypto::getLSB(B);

            tweak1 = crypto::makeBlock(2 * idx, (std::uint64_t) 0);
            tweak2 = crypto::makeBlock(2 * idx + 1, (std::uint64_t) 0);

            {
                crypto::block keys[2];
                crypto::block masks[2];

                keys[0] = crypto::xorBlocks(crypto::double_block(A), tweak1);
                keys[1] = crypto::xorBlocks(crypto::double_block(B), tweak2);
                masks[0] = keys[0];
                masks[1] = keys[1];
                AES_ecb_encrypt_blks(keys, 2, key);
                HA = crypto::xorBlocks(keys[0], masks[0]);
                HB = crypto::xorBlocks(keys[1], masks[1]);
            }

            W = crypto::xorBlocks(HA, HB);
            if (sa)
                W = crypto::xorBlocks(W, table[0]);
            if (sb) {
                W = crypto::xorBlocks(W, table[1]);
                W = crypto::xorBlocks(W, A);
            }
            *out = W;
        }

        static inline void garble_gate_eval_halfgates_mitccrh(crypto::block A, crypto::block B, crypto::block *out, const crypto::block *table, crypto::MiTCCRH *mitccrh) {
            crypto::block HA, HB, W;
            int sa, sb;

            sa = crypto::getLSB(A);
            sb = crypto::getLSB(B);

            crypto::block H[2];
            mitccrh->k2_h2(A, B, H);
            HA = H[0];
            HB = H[1];

            W = crypto::xorBlocks(HA, HB);
            if (sa)
                W = crypto::xorBlocks(W, table[0]);
            if (sb) {
                W = crypto::xorBlocks(W, table[1]);
                W = crypto::xorBlocks(W, A);
            }
            *out = W;
        }

        std::int64_t global_id;
        Wire public_constants[2];

        crypto::PRP prp;
        crypto::PRG shared_prg;
    };
}

#endif
