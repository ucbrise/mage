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
            : parms(parms_from_file("parms.ckks")), context(parms), evaluator(context),
              input_reader(input_file, std::ios::binary), output_writer(output_file, std::ios::binary) {
            std::ifstream relin_file("relinkeys.ckks");
            this->relin_keys.load(this->context, relin_file);
        }

        void input(std::uint8_t* buffer, std::int32_t level) {
            if (level == -1) {
                std::cerr << "TODO: handle plaintext" << std::endl;
                std::abort();
            }
            seal::Ciphertext c;
            c.load(this->context, this->input_reader);
            this->serialize(c, buffer, level);
        }

        void output(const std::uint8_t* buffer, std::int32_t level) {
            seal::Ciphertext c;
            this->deserialize(c, buffer, level);
            c.save(this->output_writer, seal::compr_mode_type::none);
        }

        void op_add(std::uint8_t* output, const std::uint8_t* input1, const std::uint8_t* input2, std::int32_t level) {
            seal::Ciphertext a, b;
            this->deserialize(a, input1, level);
            this->deserialize(b, input2, level);

            seal::Ciphertext c;
            this->evaluator.add(a, b, c);
            this->serialize(c, output, level);
        }

        void op_multiply(std::uint8_t* output, const std::uint8_t* input1, const std::uint8_t* input2, std::int32_t level) {
            seal::Ciphertext a, b;
            this->deserialize(a, input1, level + 1);
            this->deserialize(b, input2, level + 1);

            seal::Ciphertext c;
            this->evaluator.multiply(a, b, c);
            this->evaluator.relinearize_inplace(c, this->relin_keys);
            this->evaluator.rescale_to_next_inplace(c);
            c.scale() = ckks_scale;
            this->serialize(c, output, level);
        }

        static std::size_t level_size(std::int32_t level) {
            return ckks_ciphertext_size(level);
        }

    private:
        void serialize(seal::Ciphertext& c, std::uint8_t* buffer, std::int32_t level) {
            std::size_t buffer_size = level_size(level);
            std::size_t written = c.save(reinterpret_cast<seal::seal_byte*>(buffer), buffer_size, seal::compr_mode_type::none);
            if (written > buffer_size) {
                std::cerr << "Buffer overflow: wrote " << written << " bytes for level " << level << " but allocated " << buffer_size << " bytes" << std::endl;
                std::cerr << "Upper bound on level " << level << " ciphertext size is " << c.save_size(seal::compr_mode_type::none) << " bytes" << std::endl;
                std::abort();
            }
        }

        void deserialize(seal::Ciphertext& c, const std::uint8_t* buffer, std::int32_t level) {
            c.unsafe_load(this->context, reinterpret_cast<const seal::seal_byte*>(buffer), level_size(level));
        }

        seal::EncryptionParameters parms;
        seal::SEALContext context;
        seal::Evaluator evaluator;
        seal::RelinKeys relin_keys;

        std::ifstream input_reader;
        std::ofstream output_writer;
    };
}

#endif
