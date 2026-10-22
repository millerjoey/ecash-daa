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
constexpr int64_t MIN_SPAN{0}, MAX_SPAN{212306};
} // namespace

uint32_t CalculateTarget(uint32_t reference_bits, int64_t time_delta, int64_t /*height_delta*/, const uint256& pow_limit)
{
    const auto target = *Assert(DeriveTarget(reference_bits, pow_limit));
    const auto elapsed = std::clamp(time_delta, MIN_SPAN, MAX_SPAN);
    return ScaleTarget(target, (SMOOTHING - 1) * TIMESPAN + elapsed, SMOOTHING * TIMESPAN, UintToArith256(pow_limit)).GetCompact();
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
