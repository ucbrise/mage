/*
 * This file is based heavily on the file utils/prg.h in EMP-toolkit.
 */

#ifndef MAGE_CRYPTO_PRG_HPP_
#define MAGE_CRYPTO_PRG_HPP_

#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <openssl/err.h>
#include <openssl/rand.h>
#include "crypto/aes.hpp"

namespace mage::crypto {
    /* Pseudorandom generator implemented using AES-CTR. */
    class PRG {
    public:
        PRG(const void* seed = nullptr) : counter(0) {
            if (seed != nullptr) {
                const block* seed_block = reinterpret_cast<const block*>(seed);
                this->set_seed(*seed_block);
                return;
            }
            block v;
            int rv = RAND_bytes(reinterpret_cast<unsigned char*>(&v), sizeof(v));
            if (rv == 0) {
                ERR_print_errors_fp(stderr);
                std::abort();
            }
            this->set_seed(v);
        }

        void set_seed(const block& key) {
            AES_set_encrypt_key(key, &this->aes);
        }

        void random_block(block* data, int count = 1) {
            int i;
            for (i = 0; i < count; i++) {
                data[i] = makeBlock(0LL, this->counter++);
            }
            for(i = 0; i < count - AES_BATCH_SIZE; i += AES_BATCH_SIZE) {
                AES_ecb_encrypt_blks(data + i, AES_BATCH_SIZE, &this->aes);
            }
            AES_ecb_encrypt_blks(data + i, std::min(count - i, AES_BATCH_SIZE), &this->aes);
        }

    private:
        std::uint64_t counter;
        AES_KEY aes;
    };
}

#endif
