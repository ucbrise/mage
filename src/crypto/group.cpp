/*
 * This file is based heavily on the file utils/group_openssl.h in EMP-toolkit.
 */

#include "crypto/group.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/obj_mac.h>

namespace mage::crypto {
    static inline void openssl_fail() {
        ERR_print_errors_fp(stderr);
        std::abort();
    }

    DDHGroup::DDHGroup() {
        this->ec_group = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);
        assert(this->ec_group != nullptr);

        this->bn_ctx = BN_CTX_new();
        assert(this->bn_ctx != nullptr);

        EC_GROUP_precompute_mult(this->ec_group, this->bn_ctx);

        this->order = BN_new();
        assert(this->order != nullptr);

        EC_GROUP_get_order(this->ec_group, this->order, this->bn_ctx);
    }

    DDHGroup::~DDHGroup() {
        assert(this->ec_group != nullptr);
        EC_GROUP_free(this->ec_group);

        assert(this->bn_ctx != nullptr);
        BN_CTX_free(this->bn_ctx);
    }

    ScalarMod::ScalarMod(const DDHGroup& g) : group(g) {
        this->n = BN_new();
        assert(this->n != nullptr);
    }

    ScalarMod::ScalarMod(const ScalarMod& other) : ScalarMod(other.group) {
        BIGNUM* rv = BN_copy(this->n, other.n);
        if (rv == nullptr) {
            openssl_fail();
        }
    }

    ScalarMod::~ScalarMod() {
        assert(this->n != nullptr);
        BN_free(this->n);
    }

    void ScalarMod::set_random() {
        int rv = BN_rand_range(this->n, this->group.order);
        if (rv != 1) {
            openssl_fail();
        }
    }

    void ScalarMod::multiply(const ScalarMod& a, const ScalarMod& b) {
        int rv = BN_mod_mul(this->n, a.n, b.n, this->group.order, this->group.bn_ctx);
        if (rv != 1) {
            openssl_fail();
        }
    }

    DDHGroupElement::DDHGroupElement(const DDHGroup& g) : group(g) {
        this->point = EC_POINT_new(this->group.ec_group);
        assert(this->point != nullptr);
    }

    DDHGroupElement::DDHGroupElement(const DDHGroupElement& other) : DDHGroupElement(other.group) {
        int rv = EC_POINT_copy(this->point, other.point);
        if (rv != 1) {
            openssl_fail();
        }
    }

    DDHGroupElement::~DDHGroupElement() {
        assert(this->point != nullptr);
        EC_POINT_free(this->point);
    }

    void DDHGroupElement::marshal_uncompressed(std::uint8_t* buffer, std::size_t length) const {
        int rv = EC_POINT_point2oct(this->group.ec_group, this->point, POINT_CONVERSION_UNCOMPRESSED, buffer, length, this->group.bn_ctx);
        assert(((std::size_t) rv) <= length);
        (void) rv;
    }

    std::size_t DDHGroupElement::marshalled_uncompressed_size() const {
        int rv = EC_POINT_point2oct(this->group.ec_group, this->point, POINT_CONVERSION_UNCOMPRESSED, nullptr, 0, this->group.bn_ctx);
        assert(rv != 0);
        return (std::size_t) rv;
    }

    void DDHGroupElement::unmarshal_uncompressed(const std::uint8_t* buffer, std::size_t length) {
        int rv = EC_POINT_oct2point(this->group.ec_group, this->point, buffer, length, this->group.bn_ctx);
        if (rv != 1) {
            openssl_fail();
        }
    }

    void DDHGroupElement::set_generator() {
        int rv = EC_POINT_copy(this->point, EC_GROUP_get0_generator(this->group.ec_group));
        if (rv != 1) {
            openssl_fail();
        }
    }

    void DDHGroupElement::add(const DDHGroupElement& a, const DDHGroupElement& __restrict b) {
        int rv = EC_POINT_add(this->group.ec_group, this->point, a.point, b.point, this->group.bn_ctx);
        if (rv != 1) {
            openssl_fail();
        }
    }

    void DDHGroupElement::multiply_generator(const ScalarMod& __restrict m) {
        int rv = EC_POINT_mul(this->group.ec_group, this->point, m.n, nullptr, nullptr, this->group.bn_ctx);
        if (rv != 1) {
            openssl_fail();
        }
    }

    void DDHGroupElement::multiply_restrict(const DDHGroupElement& __restrict base, const ScalarMod& __restrict m) {
        int rv = EC_POINT_mul(this->group.ec_group, this->point, nullptr, base.point, m.n, this->group.bn_ctx);
        if (rv != 1) {
            openssl_fail();
        }
    }

    void DDHGroupElement::invert() {
        int rv = EC_POINT_invert(this->group.ec_group, this->point, this->group.bn_ctx);
        if (rv != 1) {
            openssl_fail();
        }
    }

    bool DDHGroupElement::operator ==(const DDHGroupElement& other) {
        int rv = EC_POINT_cmp(this->group.ec_group, this->point, other.point, this->group.bn_ctx);
        if (rv == -1) {
            openssl_fail();
        }
        return rv == 0;
    }
}
