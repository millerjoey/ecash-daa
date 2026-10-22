#!/usr/bin/env python3
# Copyright (c) 2026 The eCash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Check the selected DAA specification against Decimal; regenerate C++ vectors."""
import argparse
import csv
import re
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


def read_golden():
    path = ROOT/'src/test/data/asert_vectors.csv'
    with path.open() as f:
        rows = csv.reader(line for line in f if not line.startswith('#'))
        return [tuple(int(value,0) for value in row) for row in rows]


def check_upstream(directory):
    count = 0
    for path in sorted(directory.glob('run[0-9][0-9]')):
        text = path.read_text()
        height = int(re.search(r'anchor height: (\d+)',text)[1])
        timestamp = int(re.search(r'anchor (?:ancestor|parent) time: (-?\d+)',text)[1])
        bits = int(re.search(r'anchor nBits: (0x[0-9a-fA-F]+)',text)[1],16)
        for line in text.splitlines():
            if not line or line.startswith('#'):
                continue
            _,h,t,expected = line.split()
            assert daa.calculate(bits,int(t)-timestamp,int(h)-height,daa.decode(0x1d00ffff)) == int(expected,16),(path.name,line)
            count += 1
    assert count > 0
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write-vectors',action='store_true')
    parser.add_argument('--upstream-dir',type=Path,help='Optional directory containing all twelve published ASERT vector runs')
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
        assert abs(Decimal(actual)-real) <= max(Decimal(1),real*daa.RELATIVE_TOLERANCE), (bits,s,h)
        assert daa.compact(int(daa.real_target(bits,s,h,precision=120))) == daa.compact(int(daa.real_target(bits,s,h,precision=170)))
    # Check the entire Q16 polynomial domain, including its unsigned bound.
    previous = 65536
    for f in range(65536):
        polynomial = 195766423245049*f + 971821376*f*f + 5127*f*f*f + (1 << 47)
        assert polynomial < 1 << 64
        factor = 65536 + (polynomial >> 48)
        assert factor >= previous
        previous = factor
    golden = read_golden()
    for bits,seconds,heights,expected in golden:
        assert daa.calculate(bits,seconds,heights,daa.decode(0x1d00ffff)) == expected
    upstream_count = check_upstream(args.upstream_dir) if args.upstream_dir else 0
    simulation_retargets = 0
    for profile in ('constant','step','oscillating'):
        rng = random.Random(144)
        bits, timestamp = 0x1c123456, 1_600_000_000
        anchor_bits, parent_time = bits, timestamp-600
        for period in range(42):
            rate = 1 if profile == 'constant' else (2 if 14 <= period < 28 else 0.5) if profile == 'step' else (2 if period % 2 else 0.5)
            for j in range(144):
                mean = 600*daa.decode(anchor_bits)/(rate*daa.decode(bits))
                timestamp += max(1,int(rng.expovariate(1/mean)))
                heights = period*144+j+1
                value = daa.integer_target(anchor_bits,timestamp-parent_time,heights)
                real = daa.real_target(anchor_bits,timestamp-parent_time,heights)
                assert abs(Decimal(value)-real) <= max(Decimal(1),real*daa.RELATIVE_TOLERANCE)
                bits = daa.compact(value)
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
'''+'\n'.join(rows)+'\n};\n'+f'inline constexpr bool DAA_ANCHORED{{true}};\ninline constexpr uint32_t DAA_FIRST_FAST_BITS{{0x{daa.calculate(0x207fffff,600,1):08x}U}};\n#endif\n'
        (ROOT/'src/test/daa_vectors.h').write_text(text)
        golden_header = '// Published ASERT vector subset; see data/asert_vectors.README.md.\n#ifndef BITCOIN_TEST_DAA_ASERT_VECTORS_H\n#define BITCOIN_TEST_DAA_ASERT_VECTORS_H\n#include <cstdint>\nstruct ASERTVector { uint32_t bits; int64_t seconds; int64_t heights; uint32_t expected; };\ninline constexpr ASERTVector ASERT_VECTORS[]{\n'
        golden_header += ''.join(f'    {{0x{bits:08x}U, {seconds}LL, {heights}LL, 0x{expected:08x}U}},\n' for bits,seconds,heights,expected in golden)
        golden_header += '};\n#endif\n'
        (ROOT/'src/test/daa_asert_vectors.h').write_text(golden_header)
    print(f'ASERT: {len(samples)} Decimal checks; {len(vectors)} numerical vectors; {len(golden)} published vectors; {upstream_count} full upstream vectors; {simulation_retargets} simulated retargets; 65536 polynomial fractions')


if __name__ == '__main__':
    main()
