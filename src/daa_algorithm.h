// Copyright (c) 2026 The eCash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DAA_ALGORITHM_H
#define BITCOIN_DAA_ALGORITHM_H

#include <daa.h>

class CBlockIndex;

namespace daa {
/** reference_bits is the preceding target for partial retargets, or the fixed
 * anchor target for anchored exponential. height_delta is relative to that
 * anchor (144 for a partial retarget). Inputs come from validated ancestors. */
uint32_t CalculateTarget(uint32_t reference_bits, int64_t time_delta, int64_t height_delta, const uint256& pow_limit);
uint32_t GetWorkRequired(const CBlockIndex& last, const Consensus::Params& params);
bool PermittedTransition(const Consensus::Params& params, int64_t height, uint32_t old_bits, uint32_t new_bits);
} // namespace daa

#endif // BITCOIN_DAA_ALGORITHM_H
