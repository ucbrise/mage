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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <seal/seal.h>
#include "util/binaryfile.hpp"

double ckks_scale = std::pow(2.0, 40);

void check_num_args(int argc, int expected) {
    if (argc != expected) {
        std::cerr << "Need " << expected << " arguments" << std::endl;
        std::abort();
    }
}

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

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [command]" << std::endl;
        return EXIT_FAILURE;
    }

    if (std::strcmp(argv[1], "keygen") == 0) {
        check_num_args(argc, 2);

        seal::EncryptionParameters parms(seal::scheme_type::ckks);
        size_t poly_modulus_degree = 8192;
        parms.set_poly_modulus_degree(poly_modulus_degree);
        parms.set_coeff_modulus(seal::CoeffModulus::Create(poly_modulus_degree, { 60, 40, 40, 60 }));
        {
            std::ofstream parms_file("parms.ckks", std::ios::binary);
            parms.save(parms_file);
        }

        seal::SEALContext context(parms);
        seal::KeyGenerator keygen(context);
        {
            auto secret_key = keygen.secret_key();
            std::ofstream sk_file("secretkey.ckks", std::ios::binary);
            secret_key.save(sk_file);
        }
        {
            seal::PublicKey public_key;
            keygen.create_public_key(public_key);
            std::ofstream pk_file("publickey.ckks", std::ios::binary);
            public_key.save(pk_file);
        }
        {
            seal::RelinKeys relin_keys;
            keygen.create_relin_keys(relin_keys);
            std::ofstream rk_file("relinkeys.ckks", std::ios::binary);
            relin_keys.save(rk_file);
        }
        {
            seal::GaloisKeys gal_keys;
            keygen.create_galois_keys(gal_keys);
            std::ofstream gk_file("galoiskeys.ckks", std::ios::binary);
            gal_keys.save(gk_file);
        }
    } else if (std::strcmp(argv[1], "encrypt_batch") == 0) {
        if (argc < 4) {
            std::cerr << "Usage: " << argv[0] << " encrypt_batch filename double1 [double2] ..." << std::endl;
            std::abort();
        }

        seal::EncryptionParameters parms = parms_from_file("parms.ckks");
        seal::SEALContext context(parms);
        seal::PublicKey public_key = from_file<seal::PublicKey>(context, "publickey.ckks");
        seal::Encryptor encryptor(context, public_key);

        std::vector<double> batch_data;
        for (int i = 3; i != argc; i++) {
            double item = std::stod(argv[i]);
            batch_data.push_back(item);
        }

        seal::Plaintext plaintext;
        seal::CKKSEncoder encoder(context);
        encoder.encode(batch_data, ckks_scale, plaintext);

        seal::Ciphertext ciphertext;
        encryptor.encrypt(plaintext, ciphertext);

        {
            std::cout << "Up to " << ciphertext.save_size() << " bytes" << std::endl;
            std::ofstream ciphertext_file(argv[2], std::ios::binary);
            ciphertext.save(ciphertext_file);
        }
    } else if (std::strcmp(argv[1], "encrypt_file") == 0) {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " encrypt_file batch_size level [file1] ..." << std::endl;
            std::abort();
        }
        seal::EncryptionParameters parms = parms_from_file("parms.ckks");
        seal::SEALContext context(parms);
        seal::PublicKey public_key = from_file<seal::PublicKey>(context, "publickey.ckks");
        seal::Encryptor encryptor(context, public_key);

        int batch_size = std::stoi(argv[2]);
        if (batch_size <= 0) {
            std::cerr << "Batch size must be positive" << std::endl;
            std::abort();
        }

        int level = std::stoi(argv[3]);
        if (level < 0) {
            std::cerr << "Level must be nonnegative" << std::endl;
            std::abort();
        }
        auto context_data = context.first_context_data();
        while (context_data->chain_index() > level) {
            context_data = context_data->next_context_data();
        }
        if (context_data->chain_index() != level) {
            std::cout << "Could not find params for level " << level << " (max level is " << context.first_context_data()->chain_index() << ")" << std::endl;
            std::abort();
        }
        auto target_level_parms_id = context_data->parms_id();

        seal::CKKSEncoder encoder(context);

        for (int i = 4; i != argc; i++) {
            std::string filename(argv[i]);

            std::string temp_name = filename + ".plain";
            std::filesystem::rename(filename, temp_name);

            std::ofstream target(argv[i], std::ios::binary);

            std::vector<double> batch_data;

            mage::util::BinaryFileReader reader(temp_name.c_str());
            std::uint64_t num_bytes = reader.get_file_length();
            std::uint64_t num_uint32s = num_bytes >> 2;
            for (std::uint64_t i = 0; i != num_uint32s; i++) {
                double value = reader.BinaryReader::read<float>();
                batch_data.push_back(value);
                if (batch_data.size() == batch_size || i + 1 == num_uint32s) {
                    seal::Plaintext plaintext;
                    encoder.encode(batch_data, target_level_parms_id, ckks_scale, plaintext);

                    seal::Ciphertext ciphertext;
                    encryptor.encrypt(plaintext, ciphertext);
                    ciphertext.save(target);


                    batch_data.clear();
                }
            }
        }
    } else if (std::strcmp(argv[1], "decrypt_batch") == 0) {
        check_num_args(argc, 3);

        seal::EncryptionParameters parms = parms_from_file("parms.ckks");
        seal::SEALContext context(parms);
        seal::SecretKey secret_key = from_file<seal::SecretKey>(context, "secretkey.ckks");
        seal::Decryptor decryptor(context, secret_key);

        seal::Ciphertext ciphertext = from_file<seal::Ciphertext>(context, argv[2]);

        seal::Plaintext plaintext;
        decryptor.decrypt(ciphertext, plaintext);

        seal::CKKSEncoder encoder(context);
        std::vector<double> batch_data;
        encoder.decode(plaintext, batch_data);
        for (std::size_t i = 0; i != batch_data.size(); i++) {
            std::cout << batch_data[i];
            if (i + 1 == batch_data.size()) {
                std::cout << std::endl;
            } else {
                std::cout << " ";
            }
        }
    } else if (std::strcmp(argv[1], "decrypt_file") == 0) {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " decrypt_file batch_size [file1] ..." << std::endl;
            std::abort();
        }

        seal::EncryptionParameters parms = parms_from_file("parms.ckks");
        seal::SEALContext context(parms);
        seal::SecretKey secret_key = from_file<seal::SecretKey>(context, "secretkey.ckks");
        seal::Decryptor decryptor(context, secret_key);

        int batch_size = std::stoi(argv[2]);
        if (batch_size <= 0) {
            std::cerr << "Batch size must be positive" << std::endl;
            std::abort();
        }

        seal::CKKSEncoder encoder(context);

        for (int i = 3; i != argc; i++) {
            std::string filename(argv[i]);

            std::string temp_name = filename + ".ciphertext";
            std::filesystem::rename(filename, temp_name);

            std::ifstream source(temp_name, std::ios::binary);

            std::vector<double> batch_data;

            mage::util::BinaryFileWriter writer(argv[i]);
            while (source.peek() != EOF) {
                seal::Ciphertext ciphertext;
                ciphertext.load(context, source);
                seal::Plaintext plaintext;
                decryptor.decrypt(ciphertext, plaintext);
                encoder.decode(plaintext, batch_data);
                for (std::size_t j = 0; j < batch_data.size() && j < batch_size; j++) {
                    writer.write_float((float) batch_data[j]);
                }
                batch_data.clear();
            }
        }
    } else if (std::strcmp(argv[1], "switch") == 0) {
        check_num_args(argc, 4);

        seal::EncryptionParameters parms = parms_from_file("parms.ckks");
        seal::SEALContext context(parms);
        seal::Evaluator evaluator(context);

        seal::Ciphertext a = from_file<seal::Ciphertext>(context, argv[3]);
        seal::Ciphertext b;
        evaluator.mod_switch_to_next(a, b);
        b.scale() = ckks_scale; // the examples say to do this

        {
            std::cout << "Up to " << b.save_size() << " bytes" << std::endl;
            std::ofstream ciphertext_file(argv[2], std::ios::binary);
            b.save(ciphertext_file);

        }
    } else if (std::strcmp(argv[1], "add") == 0 || std::strcmp(argv[1], "multiply") == 0) {
        check_num_args(argc, 5);

        seal::EncryptionParameters parms = parms_from_file("parms.ckks");
        seal::SEALContext context(parms);
        seal::Evaluator evaluator(context);

        seal::Ciphertext a = from_file<seal::Ciphertext>(context, argv[3]);
        seal::Ciphertext b = from_file<seal::Ciphertext>(context, argv[4]);

        seal::Ciphertext c;
        if (std::strcmp(argv[1], "add") == 0) {
            evaluator.add(a, b, c);
        } else if (std::strcmp(argv[1], "multiply") == 0) {
            seal::RelinKeys relin_keys = from_file<seal::RelinKeys>(context, "relinkeys.ckks");
            evaluator.multiply(a, b, c);
            std::cout << "Nonlinear size: up to " << c.save_size() << " bytes" << std::endl;
            evaluator.relinearize_inplace(c, relin_keys);
            evaluator.rescale_to_next_inplace(c);
            c.scale() = ckks_scale; // the examples say to do this
        }

        {
            std::cout << "Up to " << c.save_size() << " bytes" << std::endl;
            std::ofstream ciphertext_file(argv[2], std::ios::binary);
            c.save(ciphertext_file);
        }
    } else if (std::strcmp(argv[1], "abpluscd") == 0) {
        check_num_args(argc, 8);

        seal::EncryptionParameters parms = parms_from_file("parms.ckks");
        seal::SEALContext context(parms);
        seal::Evaluator evaluator(context);

        seal::Ciphertext a = from_file<seal::Ciphertext>(context, argv[4]);
        seal::Ciphertext b = from_file<seal::Ciphertext>(context, argv[5]);
        seal::Ciphertext c = from_file<seal::Ciphertext>(context, argv[6]);
        seal::Ciphertext d = from_file<seal::Ciphertext>(context, argv[7]);

        seal::Ciphertext e;
        seal::RelinKeys relin_keys = from_file<seal::RelinKeys>(context, "relinkeys.ckks");
        std::chrono::time_point<std::chrono::steady_clock> start, end, add_start, add_end;
        {
            start = std::chrono::steady_clock::now();
            seal::Ciphertext temp1;
            seal::Ciphertext temp2;
            evaluator.multiply(a, b, temp1);
            evaluator.multiply(c, d, temp2);
            add_start = std::chrono::steady_clock::now();
            evaluator.add(temp1, temp2, e);
            add_end = std::chrono::steady_clock::now();
            evaluator.relinearize_inplace(e, relin_keys);
            evaluator.rescale_to_next_inplace(e);
            e.scale() = ckks_scale; // the examples say to do this
            end = std::chrono::steady_clock::now();
            std::chrono::microseconds us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            std::cerr << "Strategy 1: " << us.count() << " us" << std::endl;
            std::chrono::microseconds add_us = std::chrono::duration_cast<std::chrono::microseconds>(add_end - add_start);
            std::cerr << "Strategy 1 (add): " << add_us.count() << " us" << std::endl;
        }
        {
            std::cout << "Up to " << e.save_size() << " bytes" << std::endl;
            std::ofstream ciphertext_file(argv[2], std::ios::binary);
            e.save(ciphertext_file);
        }
        {
            start = std::chrono::steady_clock::now();
            seal::Ciphertext temp1;
            evaluator.multiply(a, b, temp1);
            evaluator.relinearize_inplace(temp1, relin_keys);
            evaluator.rescale_to_next_inplace(temp1);
            temp1.scale() = ckks_scale;

            seal::Ciphertext temp2;
            evaluator.multiply(c, d, temp2);
            evaluator.relinearize_inplace(temp2, relin_keys);
            evaluator.rescale_to_next_inplace(temp2);
            temp2.scale() = ckks_scale;
            end = std::chrono::steady_clock::now();
            std::chrono::microseconds us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            std::cerr << "Strategy 2: " << us.count() << " us" << std::endl;

            add_start = std::chrono::steady_clock::now();
            evaluator.add(temp1, temp2, e);
            add_end = std::chrono::steady_clock::now();
            std::chrono::microseconds add_us = std::chrono::duration_cast<std::chrono::microseconds>(add_end - add_start);
            std::cerr << "Strategy 2 (add): " << add_us.count() << " us" << std::endl;
        }
        {
            std::cout << "Up to " << e.save_size() << " bytes" << std::endl;
            std::ofstream ciphertext_file(argv[3], std::ios::binary);
            e.save(ciphertext_file);
        }
    } else if (std::strcmp(argv[1], "float_file_decode") == 0) {
        check_num_args(argc, 3);
        mage::util::BinaryFileReader reader(argv[2]);
        std::size_t total_length = reader.get_file_length();
        for (std::size_t amount_read = 0; amount_read < total_length; amount_read += 4) {
            float value = reader.BinaryReader::read<float>();
            std::cout << value << std::endl;
        }
    } else {
        std::cerr << "Unknown command " << argv[1] << std::endl;
        std::abort();
    }
}
