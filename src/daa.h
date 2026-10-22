// Copyright (c) 2026 The eCash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DAA_H
#define BITCOIN_DAA_H

#include <arith_uint256.h>
#include <consensus/params.h>

#include <cstdint>

namespace daa {
inline constexpr int64_t INTERVAL{144};
inline constexpr int64_t SPACING{600};
inline constexpr int64_t TIMESPAN{INTERVAL * SPACING};
inline constexpr int64_t SMOOTHING{14};
// Synthetic test height, unrelated to the still-unset public deployment height.
inline constexpr int REGTEST_HEIGHT{200};

bool Active(const Consensus::Params& params, int64_t height);
bool Retarget(const Consensus::Params& params, int64_t height);

/** Exact floor(target * numerator / denominator), saturated to [1, limit].
 * No intermediate multiplication is allowed to wrap. Inputs must be positive. */
arith_uint256 ScaleTarget(const arith_uint256& target, uint64_t numerator, uint64_t denominator, const arith_uint256& limit);

/** Exact floor(target * factor * 2^shift), saturated to [1, limit]. */
arith_uint256 ScaleTargetPow2(const arith_uint256& target, uint64_t factor, int64_t shift, const arith_uint256& limit);
} // namespace daa

#endif // BITCOIN_DAA_H
