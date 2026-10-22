#!/usr/bin/env python3
# Copyright (c) 2026 The eCash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Check the selected DAA specification against Decimal; regenerate C++ vectors."""
import argparse
from decimal import Decimal
from pathlib import Path
import random
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'test/functional'))
from test_framework import daa  # noqa: E402


def vector_inputs():
    values = []
    for bits in [0x207fffff,0x1c123456,0x01010000,0x02008000]:
        for s in [-1000000,0,21599,21600,21601,43199,43200,86399,86400,86401,212305,212306,212307,345599,345600,345601,2**63-1,-2**63]:
            values.append((bits,s,144))
    rng = random.Random(973728)
    for _ in range(120):
        values.append((rng.choice([0x207fffff,0x1c123456]),rng.randrange(-3000000,10000000),rng.randrange(144,30000)))
    for h in [0,1,144,288,100000,2147483647]:
        values.extend((0x207fffff,600*h+e,h) for e in [-309657600,-1000000,-1,0,1,1000000,309657600])
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write-vectors',action='store_true')
    args = parser.parse_args()
    vectors = vector_inputs()
    samples = list(vectors)
    rng = random.Random(144)
    samples.extend((rng.choice([0x207fffff,0x1c123456]),rng.randrange(-30000000,30000000),rng.randrange(144,40000)) for _ in range(2000))
    for bits,s,h in samples:
        actual = daa.integer_target(bits,s,h)
        real = daa.real_target(bits,s,h)
        # Separately account for the final integer floor. This checks arithmetic
        # before compact encoding, whose mantissa would hide approximation error.
        assert abs(Decimal(actual)-real) <= max(Decimal(1),real*Decimal('5e-10')), (bits,s,h)
        assert daa.compact(int(daa.real_target(bits,s,h,precision=120))) == daa.compact(int(daa.real_target(bits,s,h,precision=170)))
    # Seeded PoW waiting-time simulations: target feedback acts on block times,
    # while the hashrate profile is external. This is a behavioral regression,
    # not a miner-response or adversarial security proof.
    simulation_retargets = 0
    for profile in ('constant','step','oscillating'):
        rng = random.Random(144)
        bits, times = 0x1c123456, 1_600_000_000
        anchor_bits, anchor_time = bits, times
        history = [times-(10-i)*600 for i in range(11)]
        boundary = history[5]
        for period in range(42):
            rate = 1 if profile == 'constant' else (2 if period >= 14 and period < 28 else 0.5) if profile == 'step' else (2 if period % 2 else 0.5)
            for _ in range(144):
                mean = 600*daa.decode(anchor_bits)/(rate*daa.decode(bits))
                times += max(1,int(rng.expovariate(1/mean)))
                history.append(times)
            measured = times if daa.ALGORITHM == 'anchored' else sorted(history[-11:])[5]
            delta, heights = (times-anchor_time,(period+1)*144) if daa.ALGORITHM == 'anchored' else (measured-boundary,144)
            reference = anchor_bits if daa.ALGORITHM == 'anchored' else bits
            value = daa.integer_target(reference,delta,heights)
            real = daa.real_target(reference,delta,heights)
            assert abs(Decimal(value)-real) <= max(Decimal(1),real*Decimal('5e-10'))
            bits, boundary = daa.compact(value), measured
            assert 0 < daa.decode(bits) <= daa.POW_LIMIT
            simulation_retargets += 1
    if args.write_vectors:
        rows = []
        for bits,s,h in vectors:
            literal = 'std::numeric_limits<int64_t>::min()' if s == -2**63 else f'{s}LL'
            rows.append(f'    {{0x{bits:08x}U, {literal}, {h}LL, 0x{daa.calculate(bits,s,h):08x}U}},')
        text = '''// Generated from the independent Python integer specification; also checked
// against a 120-digit Decimal oracle. Regenerate with contrib/daa_reference.py.
#ifndef BITCOIN_TEST_DAA_VECTORS_H
#define BITCOIN_TEST_DAA_VECTORS_H
#include <cstdint>
#include <limits>
struct DAAVector { uint32_t bits; int64_t seconds; int64_t heights; uint32_t expected; };
inline constexpr DAAVector DAA_VECTORS[]{
'''+'\n'.join(rows)+'\n};\n'+f'inline constexpr bool DAA_ANCHORED{{{str(daa.ALGORITHM=="anchored").lower()}}};\ninline constexpr uint32_t DAA_FIRST_FAST_BITS{{0x{daa.calculate(0x207fffff,43200,144):08x}U}};\n#endif\n'
        (ROOT/'src/test/daa_vectors.h').write_text(text)
    print(f'{daa.ALGORITHM}: {len(samples)} high-precision checks passed; {len(vectors)} C++ vectors; {simulation_retargets} simulated retargets')


if __name__ == '__main__':
    main()
