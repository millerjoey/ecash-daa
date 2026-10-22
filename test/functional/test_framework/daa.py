# Copyright (c) 2026 The eCash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""ASERT integer specification and independent Decimal oracle.

One compiled algorithm per patch; ALGORITHM is a test label, not a node flag.
"""
from decimal import Decimal, localcontext

ALGORITHM = 'asert'
INTERVAL, SPACING, TIMESPAN, K, H = 144, 600, 86400, 14, 200
HALF_LIFE = 172800
POW_LIMIT = (1 << 255) - 1
RELATIVE_TOLERANCE = Decimal('1.5e-4')

def decode(bits):
    size, word = bits >> 24, bits & 0x7fffff
    assert not bits & 0x800000
    return word >> (8 * (3-size)) if size <= 3 else word << (8 * (size-3))

def compact(target):
    size = (target.bit_length()+7)//8
    word = target << (8*(3-size)) if size <= 3 else target >> (8*(size-3))
    if word & 0x800000:
        word >>= 8
        size += 1
    return (size << 24) | word

def integer_target(bits, seconds, heights, limit=POW_LIMIT):
    error = seconds-SPACING*(heights+1)
    if error >= 256*HALF_LIFE:
        return limit
    if error <= -256*HALF_LIFE:
        return 1
    magnitude = abs(error)*65536//HALF_LIFE
    exponent = -magnitude if error < 0 else magnitude
    shifts, fraction = divmod(exponent,65536)
    factor = 65536 + ((195766423245049*fraction + 971821376*fraction**2 +
                      5127*fraction**3 + (1 << 47)) >> 48)
    value = decode(bits)*factor
    shift = shifts-16
    value = value << shift if shift >= 0 else value >> -shift
    return min(limit,max(1,value))

def calculate(bits, seconds, heights, limit=POW_LIMIT):
    return compact(integer_target(bits,seconds,heights,limit))

def real_target(bits, seconds, heights, limit=POW_LIMIT, precision=120):
    with localcontext() as ctx:
        ctx.prec = precision
        error = seconds-SPACING*(heights+1)
        if error >= 256*HALF_LIFE:
            return Decimal(limit)
        if error <= -256*HALF_LIFE:
            return Decimal(1)
        value = Decimal(decode(bits))*(Decimal(2).ln()*Decimal(error)/HALF_LIFE).exp()
        return min(Decimal(limit),max(Decimal(1),value))

def next_bits(bits,times,height):
    if height <= H:
        return bits[-1]
    last = height-1
    return calculate(bits[H-1],times[last]-times[H-2],last-(H-1))
