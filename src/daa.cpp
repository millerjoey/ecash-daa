// Copyright (c) 2026 The eCash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <daa.h>
#include <daa_math.h>

#include <algorithm>
#include <cassert>

namespace daa {
bool Active(const Consensus::Params& params, int64_t height)
{
    assert(!params.DAAHeight || (*params.DAAHeight >= 2 && *params.DAAHeight > params.EcashHeight));
    return params.DAAHeight && height >= *params.DAAHeight;
}

bool Retarget(const Consensus::Params& params, int64_t height)
{
    return Active(params, height) && height > *params.DAAHeight;
}

arith_uint256 ScaleTarget(const arith_uint256& target, uint64_t numerator, uint64_t denominator, const arith_uint256& limit)
{
    assert(target != 0 && numerator != 0 && denominator != 0 && limit != 0);
    const arith_uint256 factor{numerator}, divisor{denominator};
    const arith_uint256 quotient = target / divisor;
    // remainder < denominator <= 2^64-1, so remainder*factor fits 128 bits.
    const arith_uint256 remainder = target - quotient * divisor;
    const arith_uint256 tail = remainder * factor / divisor;
    if (quotient > limit / factor) return limit;
    arith_uint256 result = quotient * factor;
    if (tail > limit - result) return limit;
    result += tail;
    return std::max(result, arith_uint256{1});
}

arith_uint256 ScaleTargetPow2(const arith_uint256& target, uint64_t factor, int64_t shift, const arith_uint256& limit)
{
    assert(target != 0 && factor != 0 && limit != 0);
    // target*factor has at most 320 bits.
    detail::Uint512 product{target};
    product.Multiply(factor);
    if (shift < 0) {
        if (shift <= -512) return arith_uint256{1};
        product.ShiftRight(static_cast<unsigned>(-shift));
    }
    if (!product.Fits256()) return limit;
    arith_uint256 result = product.Low256();
    if (shift > 0) {
        if (shift >= 256 || result.bits() + shift > 256) return limit;
        result <<= static_cast<unsigned>(shift);
    }
    return std::clamp(result, arith_uint256{1}, limit);
}
} // namespace daa
