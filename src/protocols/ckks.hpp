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

#ifndef MAGE_PROTOCOLS_CKKS_HPP_
#define MAGE_PROTOCOLS_CKKS_HPP_

#include <cstdint>
#include <iostream>
#include <fstream>
#include <memory>
#include <seal/seal.h>
#include "protocols/ckks_constants.hpp"
#include "util/misc.hpp"
#include "util/stats.hpp"

namespace mage::protocols::ckks {
    seal::EncryptionParameters parms_from_file(const char* filename) {
        seal::EncryptionParameters parms;
        std::ifstream parms_file(filename, std::ios::binary);
        parms.load(parms_file);
        return parms;
    }

    template <typename T>
    T from_file(const seal::SEALContext& context, const char* filename) {
        T t;
        std::ifstream t_file(filename, std::ios::binary);
        t.load(context, t_file);
        return t;
    }

    class CKKSEngine {
    public:
        using Wire = std::uint8_t;

        CKKSEngine(const char* input_file, const char* output_file)
            : parms(parms_from_file("parms.ckks")), context(parms), evaluator(context), encoder(context),
              input_reader(input_file, std::ios::binary), output_writer(output_file, std::ios::binary),
              serialize_stats("CKKS-SERIALIZE", true), deserialize_stats("CKKS-DESERIALIZE", true) {
            std::ifstream relin_file("relinkeys.ckks");
            this->relin_keys.load(this->context, relin_file);
        }

        void print_stats() {
            std::cout << this->serialize_stats << std::endl;
            std::cout << this->deserialize_stats << std::endl;
        }

        void input(std::uint8_t* buffer, std::int32_t level, bool normalized) {
            if (level == -1) {
                std::cerr << "TODO: handle plaintext" << std::endl;
                std::abort();
            }
            seal::Ciphertext c;
            c.load(this->context, this->input_reader);
            this->serialize(c, buffer, level, normalized);
        }

        void output(const std::uint8_t* buffer, std::int32_t level, bool normalized) {
            seal::Ciphertext c;
            this->deserialize(c, buffer, level, normalized);
            c.save(this->output_writer, seal::compr_mode_type::none);
        }

        void op_add(std::uint8_t* output, const std::uint8_t* input1, const std::uint8_t* input2, std::int32_t level, bool normalized) {
            seal::Ciphertext a, b;
            this->deserialize(a, input1, level, normalized);
            this->deserialize(b, input2, level, normalized);

            seal::Ciphertext c;
            this->evaluator.add(a, b, c);
            this->serialize(c, output, level, normalized);
        }

        void op_sub(std::uint8_t* output, const std::uint8_t* input1, const std::uint8_t* input2, std::int32_t level, bool normalized) {
            seal::Ciphertext a, b;
            this->deserialize(a, input1, level, normalized);
            this->deserialize(b, input2, level, normalized);

            seal::Ciphertext c;
            this->evaluator.sub(a, b, c);
            this->serialize(c, output, level, normalized);
        }

        void op_multiply(std::uint8_t* output, const std::uint8_t* input1, const std::uint8_t* input2, std::int32_t level) {
            seal::Ciphertext c;

            seal::Ciphertext a;
            this->deserialize(a, input1, level + 1, true);

            if (input1 == input2) {
                this->evaluator.square(a, c);
            } else {
                seal::Ciphertext b;
                this->deserialize(b, input2, level + 1, true);
                this->evaluator.multiply(a, b, c);
            }

            this->evaluator.relinearize_inplace(c, this->relin_keys);
            this->evaluator.rescale_to_next_inplace(c);
            c.scale() = ckks_scale;
            this->serialize(c, output, level, true);
        }

        void op_multiply_plaintext(std::uint8_t* output, const std::uint8_t* input1, const std::uint8_t* input2, std::int32_t level) {
            seal::Ciphertext a;
            this->deserialize(a, input1, level + 1, true);

            seal::Plaintext b;
            this->deserialize(b, input2, level + 1);

            seal::Ciphertext c;
            this->evaluator.multiply_plain(a, b, c);
            this->evaluator.relinearize_inplace(c, this->relin_keys);
            this->evaluator.rescale_to_next_inplace(c);
            c.scale() = ckks_scale;
            this->serialize(c, output, level, true);
        }

        void op_multiply_raw(std::uint8_t* output, const std::uint8_t* input1, const std::uint8_t* input2, std::int32_t level) {
            seal::Ciphertext c;

            seal::Ciphertext a;
            this->deserialize(a, input1, level, true);

            if (input1 == input2) {
                this->evaluator.square(a, c);
            } else {
                seal::Ciphertext b;
                this->deserialize(b, input2, level, true);
                this->evaluator.multiply(a, b, c);
            }
            this->serialize(c, output, level, false);
        }

        void op_normalize(std::uint8_t* output, const std::uint8_t* input, std::int32_t level) {
            seal::Ciphertext c;
            this->deserialize(c, input, level + 1, false);
            this->evaluator.relinearize_inplace(c, this->relin_keys);
            this->evaluator.rescale_to_next_inplace(c);
            c.scale() = ckks_scale;
            this->serialize(c, output, level, true);
        }

        void op_switch_level(std::uint8_t* output, const std::uint8_t* input1, std::int32_t level) {
            seal::Ciphertext a;
            this->deserialize(a, input1, level + 1, true);
            this->evaluator.rescale_to_next_inplace(a);
            this->serialize(a, output, level, true);
        }

        void op_encode(std::uint8_t* output, std::uint64_t value, std::int32_t level) {
            double real = *reinterpret_cast<double*>(&value);
            seal::Plaintext p;
            auto context_data = context.first_context_data();
            while (context_data->chain_index() > level) {
                context_data = context_data->next_context_data();
            }
            if (context_data->chain_index() != level) {
                std::cout << "Could not find params for level " << level << " (max level is " << context.first_context_data()->chain_index() << ")" << std::endl;
                std::abort();
            }
            auto target_level_parms_id = context_data->parms_id();
            encoder.encode(real, target_level_parms_id, ckks_scale, p);
            this->serialize(p, output, level);
        }

        static std::size_t ciphertext_size(std::int32_t level, bool normalized) {
            return ckks_ciphertext_size(level, normalized);
        }

        static std::size_t plaintext_size(std::int32_t level) {
            return ckks_plaintext_size(level);
        }

    private:
        void serialize(seal::Ciphertext& c, std::uint8_t* buffer, std::int32_t level, bool normalized) {
            auto start = std::chrono::steady_clock::now();
            std::size_t buffer_size = ciphertext_size(level, normalized);
            std::size_t written = c.save(reinterpret_cast<seal::seal_byte*>(buffer), buffer_size, seal::compr_mode_type::none);
            if (written > buffer_size) {
                std::cerr << "Buffer overflow: wrote " << written << " bytes for level " << level << " ciphertext but allocated " << buffer_size << " bytes" << std::endl;
                std::cerr << "Upper bound on level " << level << " ciphertext size is " << c.save_size(seal::compr_mode_type::none) << " bytes" << std::endl;
                std::abort();
            }
            auto end = std::chrono::steady_clock::now();
            this->serialize_stats.event(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
        }

        void serialize(seal::Plaintext& c, std::uint8_t* buffer, std::int32_t level) {
            auto start = std::chrono::steady_clock::now();
            std::size_t buffer_size = plaintext_size(level);
            std::size_t written = c.save(reinterpret_cast<seal::seal_byte*>(buffer), buffer_size, seal::compr_mode_type::none);
            if (written > buffer_size) {
                std::cerr << "Buffer overflow: wrote " << written << " bytes for level " << level << " plaintext but allocated " << buffer_size << " bytes" << std::endl;
                std::cerr << "Upper bound on level " << level << " plaintext size is " << c.save_size(seal::compr_mode_type::none) << " bytes" << std::endl;
                std::abort();
            }
            auto end = std::chrono::steady_clock::now();
            this->serialize_stats.event(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
        }

        void deserialize(seal::Ciphertext& c, const std::uint8_t* buffer, std::int32_t level, bool normalized) {
            auto start = std::chrono::steady_clock::now();
            c.unsafe_load(this->context, reinterpret_cast<const seal::seal_byte*>(buffer), ciphertext_size(level, normalized));
            auto end = std::chrono::steady_clock::now();
            this->deserialize_stats.event(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
        }

        void deserialize(seal::Plaintext& c, const std::uint8_t* buffer, std::int32_t level) {
            auto start = std::chrono::steady_clock::now();
            c.unsafe_load(this->context, reinterpret_cast<const seal::seal_byte*>(buffer), plaintext_size(level));
            auto end = std::chrono::steady_clock::now();
            this->deserialize_stats.event(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
        }

        seal::EncryptionParameters parms;
        seal::SEALContext context;
        seal::Evaluator evaluator;
        seal::CKKSEncoder encoder;
        seal::RelinKeys relin_keys;

        std::ifstream input_reader;
        std::ofstream output_writer;

        util::StreamStats serialize_stats;
        util::StreamStats deserialize_stats;
    };
}

#endif
