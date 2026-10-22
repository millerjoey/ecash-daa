// Copyright (c) 2026 The eCash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <daa.h>
#include <pow.h>
#include <test/util/setup_common.h>
#include <util/chaintype.h>

#include <boost/test/unit_test.hpp>

#include <limits>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(daa_common_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(disabled_public_parameters_and_independent_periods)
{
    for (const auto chain : {ChainType::MAIN, ChainType::TESTNET, ChainType::TESTNET4, ChainType::SIGNET, ChainType::REGTEST}) {
        const auto p = CreateChainParams(*m_node.args, chain);
        BOOST_CHECK(!p->GetConsensus().DAAHeight);
        BOOST_CHECK(!daa::Active(p->GetConsensus(), std::numeric_limits<int>::max()));
        BOOST_CHECK_EQUAL(p->GetConsensus().nPowTargetSpacing, 600);
        BOOST_CHECK_EQUAL(p->GetConsensus().DifficultyAdjustmentInterval(), chain == ChainType::REGTEST ? 144 : 2016);
        BOOST_CHECK_EQUAL(p->GetConsensus().vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period, chain == ChainType::REGTEST ? 144 : 2016);
    }
    auto p = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    p.DAAHeight = p.EcashHeight + 19;
    const int h = *p.DAAHeight;
    BOOST_CHECK(!daa::Active(p, h - 1));
    BOOST_CHECK(daa::Active(p, h));
    for (int n = h; n < h + 433; ++n) BOOST_CHECK_EQUAL(daa::Retarget(p, n), n > h);
}

BOOST_AUTO_TEST_CASE(overflow_safe_scaling)
{
    const arith_uint256 maximum = ~arith_uint256{0};
    BOOST_CHECK(daa::ScaleTarget(maximum, 1'335'506, 1'209'600, maximum) == maximum);
    BOOST_CHECK(daa::ScaleTarget(maximum, 1'123'200, 1'209'600, maximum) == maximum / 14 * 13);
    BOOST_CHECK(daa::ScaleTarget(arith_uint256{1}, 13, 14, maximum) == 1);
    BOOST_CHECK(daa::ScaleTarget(maximum, UINT64_MAX, UINT64_MAX, maximum) == maximum);
    BOOST_CHECK(daa::ScaleTargetPow2(maximum, UINT64_MAX, -64, maximum) == maximum - (maximum >> 64) - arith_uint256{1});
    BOOST_CHECK(daa::ScaleTargetPow2(maximum, 2, -1, maximum) == maximum);
    BOOST_CHECK(daa::ScaleTargetPow2(maximum, 1, -256, maximum) == 1);
    BOOST_CHECK(daa::ScaleTargetPow2(arith_uint256{1}, 1, 256, maximum) == maximum);
}

BOOST_AUTO_TEST_CASE(ecash_reset_and_headers_admission)
{
    auto p = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    p.DAAHeight = p.EcashHeight + 10'000;
    std::vector<CBlockIndex> blocks(2016);
    for (int i = 0; i < 2016; ++i) {
        blocks[i].nHeight = p.EcashHeight - 2016 + i;
        blocks[i].pprev = i ? &blocks[i - 1] : nullptr;
        blocks[i].nTime = 1'600'000'000 + i * 600;
        blocks[i].nBits = 0x170fffff;
    }
    CBlockHeader candidate;
    candidate.nTime = blocks.back().nTime + 600;
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&blocks.back(), &candidate, p), p.EcashForkBits);
    BOOST_CHECK(PermittedDifficultyTransition(p, p.EcashHeight, blocks.back().nBits, p.EcashForkBits));
    BOOST_CHECK(!PermittedDifficultyTransition(p, p.EcashHeight, blocks.back().nBits, blocks.back().nBits));
}

BOOST_AUTO_TEST_SUITE_END()
