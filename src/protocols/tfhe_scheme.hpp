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

#ifndef MAGE_PROTOCOLS_TFHE_SCHEME_HPP_
#define MAGE_PROTOCOLS_TFHE_SCHEME_HPP_

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <streambuf>
#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>

namespace mage::protocols::tfhe {
    constexpr const std::size_t tfhe_ciphertext_size = 2536;
    constexpr const int tfhe_num_temp_ciphertexts = 3;

    struct TFHECiphertext {
        std::uint8_t data[tfhe_ciphertext_size];
    };

    class TFHECiphertextReadBuffer : public std::streambuf {
    public:
        TFHECiphertextReadBuffer(const TFHECiphertext* ciphertext) {
            char* base = reinterpret_cast<char*>(const_cast<std::uint8_t*>(&ciphertext->data[0]));
            this->setg(base, base, base + sizeof(ciphertext->data));
        }
    };

    class TFHECiphertextWriteBuffer : public std::streambuf {
    public:
        TFHECiphertextWriteBuffer(TFHECiphertext* ciphertext) {
            char* base = reinterpret_cast<char*>(&ciphertext->data[0]);
            this->setp(base, base + sizeof(ciphertext->data));
        }
    };

    class TFHEScheme {
    public:
        using Wire = TFHECiphertext;

        TFHEScheme() : params(nullptr), cloud_key(nullptr), ciphertexts(nullptr) {
        }

        virtual ~TFHEScheme() {
            this->clear_params();
            this->clear_cloud_key();
            this->clear_ciphertexts();
        }

        void set_params(std::istream& params_stream) {
            this->clear_params();
            this->clear_ciphertexts();

            this->params = new_tfheGateBootstrappingParameterSet_fromStream(params_stream);
            if (this->params == nullptr) {
                std::cerr << "Out of memory (allocating TFHE params)" << std::endl;
                std::abort();
            }

            this->ciphertexts = new_gate_bootstrapping_ciphertext_array(tfhe_num_temp_ciphertexts, this->params);
            if (this->ciphertexts == nullptr) {
                std::cerr << "Out of memory (allocating TFHE ciphertexts)" << std::endl;
                std::abort();
            }
        }

        void set_cloud_key(std::istream& cloud_stream) {
            this->clear_cloud_key();

            this->cloud_key = new_tfheGateBootstrappingCloudKeySet_fromStream(cloud_stream);
            if (this->cloud_key == nullptr) {
                std::cerr << "Out of memory (allocating TFHE cloud key)" << std::endl;
                std::abort();
            }
        }

        void op_and(Wire& output, const Wire& input1, const Wire& input2) {
            this->load_ciphertexts(input1, input2);
            bootsAND(&this->ciphertexts[0], &this->ciphertexts[1], &this->ciphertexts[2], this->cloud_key);
            this->unload_ciphertext(output);
        }

        void op_xor(Wire& output, const Wire& input1, const Wire& input2) {
            this->load_ciphertexts(input1, input2);
            bootsXOR(&this->ciphertexts[0], &this->ciphertexts[1], &this->ciphertexts[2], this->cloud_key);
            this->unload_ciphertext(output);
        }

        void op_not(Wire& output, const Wire& input) {
            this->load_ciphertexts(input);
            bootsNOT(&this->ciphertexts[0], &this->ciphertexts[1], this->cloud_key);
            this->unload_ciphertext(output);
        }

        void op_xnor(Wire& output, const Wire& input1, const Wire& input2) {
            this->load_ciphertexts(input1, input2);
            bootsXNOR(&this->ciphertexts[0], &this->ciphertexts[1], &this->ciphertexts[2], this->cloud_key);
            this->unload_ciphertext(output);
        }

        void op_copy(Wire& output, const Wire& input) {
            output = input; // don't want to use copy gate --- will add more copies
        }

        void one(Wire& output) {
            bootsCONSTANT(&this->ciphertexts[0], 1, this->cloud_key);
            this->unload_ciphertext(output);
        }

        void zero(Wire& output) {
            bootsCONSTANT(&this->ciphertexts[0], 1, this->cloud_key);
            this->unload_ciphertext(output);
        }

    private:
        void clear_params() {
            if (this->params != nullptr) {
                delete_gate_bootstrapping_parameters(this->params);
                this->params = nullptr;
            }
        }

        void clear_cloud_key() {
            if (this->cloud_key != nullptr) {
                delete_gate_bootstrapping_cloud_keyset(this->cloud_key);
                this->cloud_key = nullptr;
            }
        }

        void clear_ciphertexts() {
            if (this->ciphertexts != nullptr) {
                delete_gate_bootstrapping_ciphertext_array(tfhe_num_temp_ciphertexts, this->ciphertexts);
                this->ciphertexts = nullptr;
            }
        }

        void write_ciphertext(Wire& into, LweSample* from) {
            TFHECiphertextWriteBuffer buffer(&into);
            std::ostream stream(&buffer);
            export_gate_bootstrapping_ciphertext_toStream(stream, from, this->params);
        }

        void read_ciphertext(LweSample* into, const Wire& from) {
            TFHECiphertextReadBuffer buffer(&from);
            std::istream stream(&buffer);
            import_gate_bootstrapping_ciphertext_fromStream(stream, into, this->params);
        }

        void load_ciphertexts(const Wire& input1, const Wire& input2) {
            this->read_ciphertext(&this->ciphertexts[1], input1);
            this->read_ciphertext(&this->ciphertexts[2], input2);
        }

        void load_ciphertexts(const Wire& input1) {
            this->read_ciphertext(&this->ciphertexts[1], input1);
        }

        void unload_ciphertext(Wire& output) {
            this->write_ciphertext(output, &this->ciphertexts[0]);
        }

        TFheGateBootstrappingParameterSet* params;
        TFheGateBootstrappingCloudKeySet* cloud_key;
        LweSample* ciphertexts;
    };
}

#endif
