# Copyright (c) 2026 The eCash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Independent integer specification and high-precision mathematical oracle.

ALGORITHM is fixed by the selected source patch, not a node runtime option.
"""
from decimal import Decimal, localcontext

ALGORITHM = 'linear'
INTERVAL, SPACING, TIMESPAN, K, H = 144, 600, 86400, 14, 200
POW_LIMIT = (1 << 255) - 1
LN2_Q48 = 195103586505167

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
    q = decode(bits)
    if ALGORITHM == "linear":
        s = min(212306, max(0, seconds))
        value = q * (13*TIMESPAN+s) // (14*TIMESPAN)
    elif ALGORITHM == "ratio":
        s = min(345600, max(21600, seconds))
        lo, hi = 0, 1 << 33
        rhs = s << 448
        while hi-lo > 1:
            mid = (lo+hi)//2
            if mid**14 * TIMESPAN <= rhs:
                lo = mid
            else:
                hi = mid
        value = q * lo // (1 << 32)
    else:
        error = seconds-SPACING*heights
        if error >= 256*14*TIMESPAN:
            return limit
        if error <= -256*14*TIMESPAN:
            return 1
        magnitude = (abs(error) << 80)//(14*TIMESPAN*LN2_Q48)
        exponent = -magnitude if error < 0 else magnitude
        shifts, fraction = divmod(exponent, 1 << 32)
        x = fraction*LN2_Q48//(1 << 32)
        term = factor = 1 << 48
        for i in range(1,19):
            term = term*x//((1 << 48)*i)
            factor += term
        value = q*factor
        shift = shifts-48
        value = value << shift if shift >= 0 else value >> -shift
    return min(limit, max(1,value))

def calculate(bits, seconds, heights, limit=POW_LIMIT):
    return compact(integer_target(bits, seconds, heights, limit))

def real_target(bits, seconds, heights, limit=POW_LIMIT, precision=120):
    with localcontext() as ctx:
        ctx.prec = precision
        q, t = Decimal(decode(bits)), Decimal(TIMESPAN)
        if ALGORITHM == "linear":
            value = q*(13*t+min(212306,max(0,seconds)))/(14*t)
        elif ALGORITHM == "ratio":
            s = Decimal(min(345600,max(21600,seconds)))
            value = q*((s/t).ln()/14).exp()
        else:
            error = seconds-SPACING*heights
            if error >= 256*14*TIMESPAN:
                return Decimal(limit)
            if error <= -256*14*TIMESPAN:
                return Decimal(1)
            value = q*(Decimal(error)/(14*t)).exp()
        return min(Decimal(limit),max(Decimal(1),value))

def next_bits(bits, times, height):
    if height < H:
        return bits[-1]  # Standard no-retargeting regtest, initially at powLimit.
    if height == H or (height-H)%INTERVAL:
        return bits[-1]
    last = height-1
    if ALGORITHM == "anchored":
        return calculate(bits[H-1],times[last]-times[H-1],last-(H-1))
    def mtp(h):
        window = sorted(times[max(0,h-10):h+1])
        return window[len(window)//2]
    return calculate(bits[-1],mtp(last)-mtp(last-INTERVAL),INTERVAL)
