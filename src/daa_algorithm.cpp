// Copyright (c) 2026 The eCash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <daa_algorithm.h>
#include <chain.h>
#include <pow.h>
#include <util/check.h>

#include <cassert>
#include <limits>

namespace daa {
namespace {
constexpr int64_t HALF_LIFE{172800};
constexpr int64_t RADIX{65536};
} // namespace

uint32_t CalculateTarget(uint32_t reference_bits, int64_t time_delta, int64_t height_delta, const uint256& pow_limit)
{
    const auto target = *Assert(DeriveTarget(reference_bits, pow_limit));
    const auto limit = UintToArith256(pow_limit);
    assert(height_delta >= 0 && height_delta <= std::numeric_limits<int>::max());
    // BCH ASERT uses the anchor's PARENT time, and counts one more interval.
    const int64_t ideal = (height_delta + 1) * SPACING;
    // Absolute representability bounds, not per-block adjustment clamps.
    // 2^256 exceeds every valid target; the inverse produces a target below 1.
    if (time_delta >= ideal + 256 * HALF_LIFE) return limit.GetCompact();
    if (time_delta <= ideal - 256 * HALF_LIFE) return arith_uint256{1}.GetCompact();
    const int64_t exponent = ((time_delta - ideal) * RADIX) / HALF_LIFE;
    int64_t shifts = exponent / RADIX;
    int64_t fraction = exponent % RADIX;
    if (fraction < 0) { fraction += RADIX; --shifts; }
    const uint64_t f = fraction;
    // aserti3-2d Q16 cubic and rounding constant, as specified by BCHN.
    // At f <= 65535 this entire unsigned sum fits uint64_t.
    const uint64_t factor = 65536 + ((195766423245049ULL * f + 971821376ULL * f * f +
                                      5127ULL * f * f * f + (uint64_t{1} << 47)) >> 48);
    // Widen before multiplying: regtest has a 255-bit limit, unlike BCH mainnet.
    return ScaleTargetPow2(target, factor, shifts - 16, limit).GetCompact();
}

uint32_t GetWorkRequired(const CBlockIndex& last, const Consensus::Params& params)
{
    const int64_t next = int64_t{last.nHeight} + 1;
    assert(Active(params, next));
    if (!Retarget(params, next)) return last.nBits; // H is the sole carry block.
    const auto* anchor = Assert(last.GetAncestor(*params.DAAHeight - 1));
    const auto* parent = Assert(anchor->pprev);
    return CalculateTarget(anchor->nBits, last.GetBlockTime() - parent->GetBlockTime(), int64_t{last.nHeight} - anchor->nHeight, params.powLimit);
}

bool PermittedTransition(const Consensus::Params& params, int64_t height, uint32_t old_bits, uint32_t new_bits)
{
    if (!DeriveTarget(old_bits, params.powLimit) || !DeriveTarget(new_bits, params.powLimit)) return false;
    // Anchor/timestamp context in both headers-sync phases checks exact bits.
    return Retarget(params, height) || old_bits == new_bits;
}
} // namespace daa
