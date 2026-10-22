// Copyright (c) 2026 The eCash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <daa_algorithm.h>
#include <daa_math.h>
#include <chain.h>
#include <pow.h>
#include <util/check.h>

#include <algorithm>
#include <cassert>
#include <limits>

namespace daa {
namespace {
constexpr int64_t MIN_SPAN{21600}, MAX_SPAN{345600};
constexpr uint64_t RADIX{uint64_t{1} << 32};

// The greatest integer m such that m^14 * 86400 <= s * 2^448.
// This is floor(2^32 * (s/86400)^(1/14)), without log/exp or floating point.
uint64_t Multiplier(int64_t elapsed)
{
    detail::Uint512 right;
    right.words[14] = elapsed; // s * 2^(32*14)
    uint64_t low{0}, high{uint64_t{1} << 33};
    while (high - low > 1) {
        const uint64_t middle = low + (high - low) / 2;
        detail::Uint512 left{1};
        for (int i = 0; i < SMOOTHING; ++i) left.Multiply(middle);
        left.Multiply(TIMESPAN);
        // middle < 2^33: left has fewer than 14*33+17 = 479 bits.
        if (left <= right) low = middle;
        else high = middle;
    }
    return low;
}
} // namespace

uint32_t CalculateTarget(uint32_t reference_bits, int64_t time_delta, int64_t /*height_delta*/, const uint256& pow_limit)
{
    const auto target = *Assert(DeriveTarget(reference_bits, pow_limit));
    const auto elapsed = std::clamp(time_delta, MIN_SPAN, MAX_SPAN);
    return ScaleTarget(target, Multiplier(elapsed), RADIX, UintToArith256(pow_limit)).GetCompact();
}

uint32_t GetWorkRequired(const CBlockIndex& last, const Consensus::Params& params)
{
    const int64_t next = int64_t{last.nHeight} + 1;
    assert(Active(params, next));
    if (!Retarget(params, next)) return last.nBits;
    // Reuse the previous boundary, not the first block of this period.
    const auto* first = Assert(last.GetAncestor(last.nHeight - INTERVAL));
    return CalculateTarget(last.nBits, last.GetMedianTimePast() - first->GetMedianTimePast(), INTERVAL, params.powLimit);
}

bool PermittedTransition(const Consensus::Params& params, int64_t height, uint32_t old_bits, uint32_t new_bits)
{
    const auto old_target = DeriveTarget(old_bits, params.powLimit);
    const auto new_target = DeriveTarget(new_bits, params.powLimit);
    if (!old_target || !new_target) return false;
    if (!Retarget(params, height)) return old_bits == new_bits;
    const auto minimum = *Assert(DeriveTarget(CalculateTarget(old_bits, MIN_SPAN, INTERVAL, params.powLimit), params.powLimit));
    const auto maximum = *Assert(DeriveTarget(CalculateTarget(old_bits, MAX_SPAN, INTERVAL, params.powLimit), params.powLimit));
    return *new_target >= minimum && *new_target <= maximum;
}
} // namespace daa
