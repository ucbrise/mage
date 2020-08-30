/*
 * This file is based heavily on the file utils/group.h in EMP-toolkit.
 */

#ifndef MAGE_CRYPTO_GROUP_HPP_
#define MAGE_CRYPTO_GROUP_HPP_

#include <cstddef>
#include <cstdint>
#include <openssl/bn.h>
#include <openssl/ec.h>

namespace mage::crypto {
    // class BigInt {
    // public:
    //     BigInt();
    //     BigInt(const BigInt& other);
    //     BigInt& operator =(const BigInt& other);
    //     ~BigInt();
    //
    // private:
    //     BIGNUM* n;
    // };

    class DDHGroupElement;
    class ScalarMod;

    class DDHGroup {
        friend class DDHGroupElement;
        friend class ScalarMod;

    public:
        DDHGroup();
        ~DDHGroup();

    private:
        BN_CTX* bn_ctx = nullptr;
        EC_GROUP* ec_group = nullptr;
        BIGNUM* order = nullptr;
    };

    class ScalarMod {
        friend class DDHGroupElement;
    public:
        ScalarMod(const DDHGroup& g);
        ScalarMod(const ScalarMod& other);
        ~ScalarMod();

        void set_random();
        void multiply(const ScalarMod& a, const ScalarMod& b);

    private:
        const DDHGroup& group;
        BIGNUM* n = nullptr;
    };

    class DDHGroupElement {
        friend class DDHGroup;

    public:
        DDHGroupElement(const DDHGroup& g);
        DDHGroupElement(const DDHGroupElement& other);
        DDHGroupElement& operator =(const DDHGroupElement& other);
        ~DDHGroupElement();

        void marshal_uncompressed(std::uint8_t* buffer, std::size_t length) const;
        std::size_t marshalled_uncompressed_size() const;
        void unmarshal_uncompressed(const std::uint8_t* buffer, std::size_t length);

        void set_generator();
        void add(const DDHGroupElement& a, const DDHGroupElement& __restrict b);
        void multiply_generator(const ScalarMod& __restrict m);
        void multiply_restrict(const DDHGroupElement& __restrict base, const ScalarMod& __restrict m);
        void invert();
        bool operator ==(const DDHGroupElement& other);

    private:
        const DDHGroup& group;
        EC_POINT* point = nullptr;
    };
}

#endif
