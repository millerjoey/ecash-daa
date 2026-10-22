// Copyright (c) 2026 The eCash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DAA_HEADERS_H
#define BITCOIN_DAA_HEADERS_H

#include <chain.h>
#include <daa_algorithm.h>
#include <pow.h>

#include <optional>

namespace daa {
/** Small independent state for each headers-sync phase. Tracks the anchor on
 * the streamed branch, never a global best-chain anchor. Header PoW and
 * continuity remain the caller's responsibility. */
class HeaderState {
    struct Anchor { int height; uint32_t time; uint32_t bits; };
    std::optional<Anchor> m_anchor;
    int64_t m_height;
    uint32_t m_time;
    uint32_t m_bits;

public:
    HeaderState(const Consensus::Params& params, const CBlockIndex& start)
        : m_height(start.nHeight), m_time(start.nTime), m_bits(start.nBits)
    {
        if (params.DAAHeight && start.nHeight >= *params.DAAHeight - 1) {
            const auto* anchor = start.GetAncestor(*params.DAAHeight - 1);
            assert(anchor);
            m_anchor = Anchor{anchor->nHeight, anchor->nTime, anchor->nBits};
        }
    }

    bool Check(const Consensus::Params& params, int64_t height, const CBlockHeader& header)
    {
        if (!params.DAAHeight) return true;
        if (height != m_height + 1) return false;
        if (height == *params.DAAHeight - 1) {
            if (!DeriveTarget(header.nBits, params.powLimit)) return false;
            m_anchor = Anchor{static_cast<int>(height), header.nTime, header.nBits};
        }
        if (Active(params, height)) {
            if (!m_anchor || !DeriveTarget(m_anchor->bits, params.powLimit)) return false;
            const uint32_t expected = Retarget(params, height)
                ? CalculateTarget(m_anchor->bits, int64_t{m_time} - m_anchor->time, m_height - m_anchor->height, params.powLimit)
                : m_bits;
            if (header.nBits != expected) return false;
        }
        m_height = height;
        m_time = header.nTime;
        m_bits = header.nBits;
        return true;
    }
};
} // namespace daa

#endif // BITCOIN_DAA_HEADERS_H
