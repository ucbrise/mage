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

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <tfhe/tfhe.h>
#include <tfhe/tfhe_io.h>
#include "util/binaryfile.hpp"

constexpr std::size_t ciphertext_size = 2536;

TFheGateBootstrappingParameterSet* params_from_file(const char* filename) {
    FILE* params_file = fopen(filename, "rb");
    if (params_file == nullptr) {
        std::cerr << "Could not open " << filename << std::endl;
        std::abort();
    }
    TFheGateBootstrappingParameterSet* params = new_tfheGateBootstrappingParameterSet_fromFile(params_file);
    fclose(params_file);
    return params;
 }

 TFheGateBootstrappingSecretKeySet* secret_key_from_file(const char* filename) {
     FILE* secret_key = fopen(filename, "rb");
     if (secret_key == nullptr) {
         std::cerr << "Could not open " << filename << std::endl;
         std::abort();
     }
     TFheGateBootstrappingSecretKeySet* key = new_tfheGateBootstrappingSecretKeySet_fromFile(secret_key);
     fclose(secret_key);
     return key;
 }

 TFheGateBootstrappingCloudKeySet* cloud_key_from_file(const char* filename) {
     FILE* cloud_key = fopen(filename, "rb");
     if (cloud_key == nullptr) {
         std::cerr << "Could not open " << filename << std::endl;
         std::abort();
     }
     TFheGateBootstrappingCloudKeySet* bk = new_tfheGateBootstrappingCloudKeySet_fromFile(cloud_key);
     fclose(cloud_key);
     return bk;
 }

 LweSample* ciphertext_from_file(const char* filename, const TFheGateBootstrappingParameterSet* params) {
     FILE* f = fopen(filename, "rb");
     if (f == nullptr) {
         std::cerr << "Could not open " << filename << std::endl;
         std::abort();
     }
     LweSample* c = new_gate_bootstrapping_ciphertext(params);
     import_gate_bootstrapping_ciphertext_fromFile(f, c, params);
     fclose(f);
     return c;
 }

 void ciphertext_to_file(LweSample* ciphertext, const char* filename, const TFheGateBootstrappingParameterSet* params) {
     FILE* f = fopen(filename, "wb");
     if (f == nullptr) {
         std::cerr << "Could not open " << filename << std::endl;
         std::abort();
     }
     export_gate_bootstrapping_ciphertext_toFile(f, ciphertext, params);
     fclose(f);
 }

 void check_num_args(int argc, int expected) {
     if (argc != expected) {
         std::cerr << "Need " << expected << " arguments" << std::endl;
         std::abort();
     }
 }

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [command]" << std::endl;
        return EXIT_FAILURE;
    }

    if (std::strcmp(argv[1], "keygen") == 0) {
        check_num_args(argc, 2);

        //generate a keyset
        const int minimum_lambda = 110;
        TFheGateBootstrappingParameterSet* params = new_default_gate_bootstrapping_parameters(minimum_lambda);

        FILE* params_file = fopen("params", "wb");
        export_tfheGateBootstrappingParameterSet_toFile(params_file, params);
        fclose(params_file);

        //generate a random key
        uint32_t seed[] = { 314, 1592, 657 };
        tfhe_random_generator_setSeed(seed, 3);
        TFheGateBootstrappingSecretKeySet* key = new_random_gate_bootstrapping_secret_keyset(params);

        //export the secret key to file for later use
        FILE* secret_key = fopen("secret.key", "wb");
        export_tfheGateBootstrappingSecretKeySet_toFile(secret_key, key);
        fclose(secret_key);

        //export the cloud key to a file (for the cloud)
        FILE* cloud_key = fopen("cloud.key", "wb");
        export_tfheGateBootstrappingCloudKeySet_toFile(cloud_key, &key->cloud);
        fclose(cloud_key);

        //clean up all pointers
        delete_gate_bootstrapping_secret_keyset(key);
        delete_gate_bootstrapping_parameters(params);
    } else if (std::strcmp(argv[1], "encrypt_bit") == 0) {
        check_num_args(argc, 4);

        TFheGateBootstrappingParameterSet* params = params_from_file("params");
        TFheGateBootstrappingSecretKeySet* key = secret_key_from_file("secret.key");

        int value = atoi(argv[3]);
        std::cout << "Encrypting " << value << std::endl;

        LweSample* ciphertext = new_gate_bootstrapping_ciphertext(params);
        bootsSymEncrypt(ciphertext, value, key);

        ciphertext_to_file(ciphertext, argv[2], params);

        delete_gate_bootstrapping_ciphertext(ciphertext);
        delete_gate_bootstrapping_secret_keyset(key);
        delete_gate_bootstrapping_parameters(params);
    } else if (std::strcmp(argv[1], "encrypt_file") == 0) {
        TFheGateBootstrappingParameterSet* params = params_from_file("params");
        TFheGateBootstrappingSecretKeySet* key = secret_key_from_file("secret.key");
        for (int i = 2; i != argc; i++) {
            std::string filename(argv[i]);

            std::string temp_name = filename + ".plain";
            std::filesystem::rename(filename, temp_name);

            LweSample* ciphertext = new_gate_bootstrapping_ciphertext(params);

            FILE* target = fopen(argv[i], "wb");
            mage::util::BinaryFileReader reader(temp_name.c_str());
            std::uint64_t num_bits = reader.get_file_length() << 3;
            for (std::uint64_t i = 0; i != num_bits; i++) {
                std::uint8_t bit = reader.read1();
                bootsSymEncrypt(ciphertext, bit, key);
                export_gate_bootstrapping_ciphertext_toFile(target, ciphertext, params);
            }

            fclose(target);
        }
    } else if (std::strcmp(argv[1], "decrypt_bit") == 0) {
        check_num_args(argc, 3);

        TFheGateBootstrappingParameterSet* params = params_from_file("params");
        TFheGateBootstrappingSecretKeySet* key = secret_key_from_file("secret.key");

        LweSample* ciphertext = ciphertext_from_file(argv[2], params);
        int value = bootsSymDecrypt(ciphertext, key);
        std::cout << value << std::endl;

        delete_gate_bootstrapping_ciphertext(ciphertext);
        delete_gate_bootstrapping_secret_keyset(key);
        delete_gate_bootstrapping_parameters(params);
    } else if (std::strcmp(argv[1], "decrypt_file") == 0) {
        TFheGateBootstrappingParameterSet* params = params_from_file("params");
        TFheGateBootstrappingSecretKeySet* key = secret_key_from_file("secret.key");
        for (int i = 2; i != argc; i++) {
            std::string filename(argv[i]);

            std::string temp_name = filename + ".ciphertext";
            std::filesystem::rename(filename, temp_name);

            LweSample* ciphertext = new_gate_bootstrapping_ciphertext(params);

            FILE* source = fopen(temp_name.c_str(), "rb");
            fseek(source, 0 , SEEK_END);
            long ciphertext_bytes = ftell(source);
            fseek(source, 0 , SEEK_SET);
            if (ciphertext_bytes % ciphertext_size != 0) {
                std::cerr << "Malformed ciphertext file" << std::endl;
                std::abort();
            }
            std::uint64_t num_bits = ciphertext_bytes / ciphertext_size;

            mage::util::BinaryFileWriter writer(argv[i]);
            for (std::uint64_t i = 0; i != num_bits; i++) {
                import_gate_bootstrapping_ciphertext_fromFile(source, ciphertext, params);
                std::uint8_t bit = bootsSymDecrypt(ciphertext, key);
                writer.write1(bit);
            }

            fclose(source);
        }
    } else if (std::strcmp(argv[1], "and") == 0 || std::strcmp(argv[1], "or") == 0 || std::strcmp(argv[1], "xor") == 0) {
        check_num_args(argc, 5);

        TFheGateBootstrappingParameterSet* params = params_from_file("params");
        TFheGateBootstrappingCloudKeySet* bk = cloud_key_from_file("cloud.key");

        LweSample* ciphertext_a = ciphertext_from_file(argv[3], params);
        LweSample* ciphertext_b = ciphertext_from_file(argv[4], params);
        LweSample* ciphertext_c = new_gate_bootstrapping_ciphertext(params);

        if (std::strcmp(argv[1], "and") == 0) {
            bootsAND(ciphertext_c, ciphertext_a, ciphertext_b, bk);
        } else if (std::strcmp(argv[1], "or") == 0) {
            bootsOR(ciphertext_c, ciphertext_a, ciphertext_b, bk);
        } else if (std::strcmp(argv[1], "xor") == 0) {
            bootsXOR(ciphertext_c, ciphertext_a, ciphertext_b, bk);
        } else {
            std::cerr << "Unknown operation '" << argv[1] << "'" << std::endl;
            std::abort();
        }

        ciphertext_to_file(ciphertext_c, argv[2], params);

        delete_gate_bootstrapping_ciphertext(ciphertext_a);
        delete_gate_bootstrapping_ciphertext(ciphertext_b);
        delete_gate_bootstrapping_ciphertext(ciphertext_c);
        delete_gate_bootstrapping_cloud_keyset(bk);
        delete_gate_bootstrapping_parameters(params);
    } else {
        std::cerr << "Unknown command " << argv[1] << std::endl;
        std::abort();
    }

    std::cout << "done" << std::endl;

    return 0;
 }
