// Copyright (c) 2026 The eCash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <daa_algorithm.h>
#include <chain.h>
#include <pow.h>
#include <util/check.h>

#include <algorithm>
#include <cassert>
#include <limits>

namespace daa {
namespace {
constexpr int64_t TAU{SMOOTHING * TIMESPAN};
constexpr int64_t EXPONENT_RADIX{int64_t{1} << 32};
constexpr uint64_t RADIX{uint64_t{1} << 48};
// floor(ln(2) * 2^48). This integer is part of the consensus approximation.
constexpr uint64_t LN2{195103586505167};
} // namespace

uint32_t CalculateTarget(uint32_t reference_bits, int64_t time_delta, int64_t height_delta, const uint256& pow_limit)
{
    const auto target = *Assert(DeriveTarget(reference_bits, pow_limit));
    const auto limit = UintToArith256(pow_limit);
    assert(height_delta >= 0 && height_delta <= std::numeric_limits<int>::max());
    const int64_t ideal = height_delta * SPACING;
    // Absolute representability bounds, NOT per-adjustment clamps.
    // e^256 > 2^256; even the smallest/largest input must saturate here.
    if (time_delta >= ideal + 256 * TAU) return limit.GetCompact();
    if (time_delta <= ideal - 256 * TAU) return arith_uint256{1}.GetCompact();
    const int64_t error = time_delta - ideal;
    arith_uint256 magnitude{static_cast<uint64_t>(error < 0 ? -error : error)};
    magnitude <<= 80; // 2^(48+32), at most 109 bits after the guards.
    magnitude /= arith_uint256{TAU} * arith_uint256{LN2};
    // Division truncates towards zero, including for negative exponents.
    const int64_t exponent = error < 0 ? -static_cast<int64_t>(magnitude.GetLow64()) : static_cast<int64_t>(magnitude.GetLow64());
    int64_t shifts = exponent / EXPONENT_RADIX;
    int64_t fraction = exponent % EXPONENT_RADIX;
    if (fraction < 0) { fraction += EXPONENT_RADIX; --shifts; }
    const uint64_t x = ((arith_uint256{static_cast<uint64_t>(fraction)} * arith_uint256{LN2}) >> 32).GetLow64();
    // exp(x), x in [0,ln(2)), in Q48. Each term is floored separately;
    // eighteen terms make series truncation smaller than one Q48 unit.
    uint64_t term{RADIX}, factor{RADIX};
    for (uint64_t i = 1; i <= 18; ++i) {
        term = (arith_uint256{term} * arith_uint256{x} / arith_uint256{RADIX * i}).GetLow64();
        factor += term;
    }
    return ScaleTargetPow2(target, factor, shifts - 48, limit).GetCompact();
}

uint32_t GetWorkRequired(const CBlockIndex& last, const Consensus::Params& params)
{
    const int64_t next = int64_t{last.nHeight} + 1;
    assert(Active(params, next));
    if (!Retarget(params, next)) return last.nBits;
    const auto* anchor = Assert(last.GetAncestor(*params.DAAHeight - 1));
    return CalculateTarget(anchor->nBits, last.GetBlockTime() - anchor->GetBlockTime(), int64_t{last.nHeight} - anchor->nHeight, params.powLimit);
}

bool PermittedTransition(const Consensus::Params& params, int64_t height, uint32_t old_bits, uint32_t new_bits)
{
    if (!DeriveTarget(old_bits, params.powLimit) || !DeriveTarget(new_bits, params.powLimit)) return false;
    // With no anchor/timestamps, this interface cannot impose a relative bound.
    // HeadersSyncState adds exact anchor-based checks in both phases.
    return Retarget(params, height) || old_bits == new_bits;
}
} // namespace daa
