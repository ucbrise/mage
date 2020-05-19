/*
 * This file is based heavily on the file utils/prp.h in EMP-toolkit.
 */

#ifndef MAGE_CRYPTO_PRP_HPP_
#define MAGE_CRYPTO_PRP_HPP_

#include <x86intrin.h>
#include "crypto/aes.hpp"

namespace mage::crypto {
    /* Simple AES wrapper. */
    class PRP {
    public:
        PRP(const void* seed = fix_key) {
            this->aes_set_key(_mm_loadu_si128(reinterpret_cast<const block*>(seed)));
        }

        PRP(const block& seed) {
            this->aes_set_key(seed);
        }

        void aes_set_key(const block& v) {
            AES_set_encrypt_key(v, &this->aes);
        }

    public:
        AES_KEY aes;
    };
}

#endif
