// Copyright (c) 2026 The eCash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <daa_algorithm.h>
#include <headerssync.h>
#include <pow.h>
#include <test/daa_vectors.h>
#include <test/daa_asert_vectors.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <span>
#include <vector>

namespace {
struct Chain {
    std::vector<CBlockIndex> blocks;
    std::vector<uint256> hashes;
    std::vector<CBlockHeader> headers;
    Chain(const Consensus::Params& params, int count, int spacing) : blocks(count), hashes(count), headers(count)
    {
        for (int i = 0; i < count; ++i) {
            auto& b = blocks[i];
            b.nHeight = i;
            b.nVersion = 4;
            b.pprev = i ? &blocks[i-1] : nullptr;
            b.nTime = 1'600'000'000 + i*spacing;
            CBlockHeader candidate;
            candidate.nTime = b.nTime;
            b.nBits = i ? GetNextWorkRequired(b.pprev, &candidate, params) : 0x207fffff;
            b.nChainWork = (i ? blocks[i-1].nChainWork : arith_uint256{0}) + GetBlockProof(b);
            b.BuildSkip();
            hashes[i] = b.GetBlockHeader().GetHash();
            b.phashBlock = &hashes[i];
            headers[i] = b.GetBlockHeader();
        }
    }
};
Consensus::Params TestParams(int height = daa::REGTEST_HEIGHT)
{
    CChainParams::RegTestOptions opts;
    opts.test_daa = true;
    auto params = CChainParams::RegTest(opts)->GetConsensus();
    params.DAAHeight = height;
    return params;
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(daa_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(deterministic_reference_vectors)
{
    const auto params = TestParams();
    for (const auto& v : DAA_VECTORS) {
        BOOST_TEST_CONTEXT("bits=" << v.bits << " seconds=" << v.seconds << " heights=" << v.heights) {
            BOOST_CHECK_EQUAL(daa::CalculateTarget(v.bits, v.seconds, v.heights, params.powLimit), v.expected);
            BOOST_CHECK(DeriveTarget(v.expected, params.powLimit));
        }
    }
}

BOOST_AUTO_TEST_CASE(published_asert_vectors)
{
    const uint256 limit = ArithToUint256(arith_uint256{}.SetCompact(0x1d00ffff));
    for (const auto& v : ASERT_VECTORS) {
        BOOST_TEST_CONTEXT("published bits=" << v.bits << " seconds=" << v.seconds << " heights=" << v.heights) {
            BOOST_CHECK_EQUAL(daa::CalculateTarget(v.bits, v.seconds, v.heights, limit), v.expected);
        }
    }
}

BOOST_AUTO_TEST_CASE(two_day_halflife)
{
    const auto params = TestParams();
    BOOST_CHECK_EQUAL(daa::CalculateTarget(0x1c123456, 600, 0, params.powLimit), 0x1c123456);
    BOOST_CHECK_EQUAL(daa::CalculateTarget(0x1c123456, 600+172800, 0, params.powLimit), 0x1c2468ac);
    BOOST_CHECK_EQUAL(daa::CalculateTarget(0x1c123456, 600-172800, 0, params.powLimit), 0x1c091a2b);
}

BOOST_AUTO_TEST_CASE(activation_and_per_block_adjustments)
{
    for (const int h : {2, 200, 288, 2016}) {
        const auto params = TestParams(h);
        Chain c{params, h+290, 300};
        for (int n = 0; n <= h; ++n) BOOST_CHECK_EQUAL(c.blocks[n].nBits, 0x207fffff);
        BOOST_CHECK_EQUAL(c.blocks[h+1].nBits, DAA_FIRST_FAST_BITS);
        for (int r = 1; r < 290; ++r) {
            const auto expected = daa::CalculateTarget(0x207fffff, (r+1)*300, r, params.powLimit);
            BOOST_CHECK_EQUAL(c.blocks[h+r].nBits, expected);
        }
        auto& last = c.blocks[h+143];
        const auto median = last.GetMedianTimePast();
        last.nTime -= 400;
        BOOST_CHECK_LT(last.nTime, last.pprev->nTime);
        BOOST_CHECK_GT(last.GetBlockTime(), last.pprev->GetMedianTimePast());
        BOOST_CHECK_EQUAL(last.GetMedianTimePast(), median);
        const auto expected = daa::CalculateTarget(0x207fffff, 145*300-400, 144, params.powLimit);
        BOOST_CHECK_EQUAL(daa::GetWorkRequired(last, params), expected);
        // The timestamp reference is the anchor's parent, not the anchor.
        c.blocks[h-1].nTime += 1000;
        BOOST_CHECK_EQUAL(daa::GetWorkRequired(last, params), expected);
        c.blocks[h-2].nTime += 1000;
        BOOST_CHECK_EQUAL(daa::GetWorkRequired(last, params), daa::CalculateTarget(0x207fffff, 145*300-1400, 144, params.powLimit));
    }
}

BOOST_AUTO_TEST_CASE(transition_checks_and_regtest_escape_is_disabled)
{
    const auto params = TestParams();
    Chain c{params, 345, 300};
    CBlockHeader late, normal;
    late.nTime = c.blocks[344].nTime + 100'000;
    normal.nTime = c.blocks[344].nTime + 300;
    const auto expected = daa::GetWorkRequired(c.blocks[344], params);
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&c.blocks[344], &late, params), expected);
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&c.blocks[344], &normal, params), expected);
    BOOST_CHECK_NE(expected, 0x207fffff);
    BOOST_CHECK(PermittedDifficultyTransition(params, 201, c.blocks[200].nBits, c.blocks[201].nBits));
    BOOST_CHECK(!PermittedDifficultyTransition(params, 200, c.blocks[199].nBits, c.blocks[199].nBits-1));
    for (uint32_t invalid : {0U, 0x20800001U, 0x23000001U, 0x2100ffffU})
        BOOST_CHECK(!PermittedDifficultyTransition(params, 201, c.blocks[200].nBits, invalid));
    const uint32_t harder = daa::CalculateTarget(0x207fffff, 0, 5000, params.powLimit);
    BOOST_CHECK(PermittedDifficultyTransition(params, 201, 0x207fffff, harder));
}

BOOST_AUTO_TEST_CASE(headers_presync_and_redownload_across_activation)
{
    const auto params = TestParams();
    Chain c{params, 701, 300};
    for (const int start : {0, 198, 199, 200, 217, 400}) {
        HeadersSyncState state{0, params, HeadersSyncParams{17, 32}, c.blocks[start], c.blocks.back().nChainWork};
        const auto headers = std::span{c.headers}.subspan(start+1);
        const auto first = state.ProcessNextHeaders(headers, true);
        BOOST_REQUIRE(first.success);
        BOOST_REQUIRE(state.GetState() == HeadersSyncState::State::REDOWNLOAD);
        const auto second = state.ProcessNextHeaders(headers, true);
        BOOST_REQUIRE(second.success);
        BOOST_CHECK_EQUAL(second.pow_validated_headers.size(), headers.size());
    }
    if (DAA_ANCHORED) {
        auto bad = c.headers;
        bad[344].nBits -= 1; // valid target, wrong anchored result
        HeadersSyncState presync{0, params, HeadersSyncParams{17,32}, c.blocks[0], c.blocks.back().nChainWork};
        BOOST_CHECK(!presync.ProcessNextHeaders(std::span{bad}.subspan(1), true).success);
        HeadersSyncState redownload{0, params, HeadersSyncParams{17,32}, c.blocks[0], c.blocks.back().nChainWork};
        BOOST_REQUIRE(redownload.ProcessNextHeaders(std::span{c.headers}.subspan(1), true).success);
        BOOST_CHECK(!redownload.ProcessNextHeaders(std::span{bad}.subspan(1), true).success);
    }
}

BOOST_AUTO_TEST_SUITE_END()
