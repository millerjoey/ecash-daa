// Copyright (c) 2026 The eCash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DAA_MATH_H
#define BITCOIN_DAA_MATH_H

#include <arith_uint256.h>

#include <array>
#include <cassert>
#include <cstdint>

namespace daa::detail {
/** Local bounded intermediate, not a replacement for arith_uint256.
 * Callers prove products fit 512 bits. Uses only standard 32/64-bit operations. */
struct Uint512 {
    std::array<uint32_t, 16> words{};

    explicit Uint512(uint64_t value = 0) { words[0] = value; words[1] = value >> 32; }
    explicit Uint512(arith_uint256 value)
    {
        for (size_t i = 0; i < 8; ++i, value >>= 32) words[i] = value.GetLow64();
    }

    void Multiply(uint64_t factor)
    {
        std::array<uint32_t, 16> result{};
        for (size_t j = 0; j < 2; ++j) {
            const uint32_t digit = factor >> (32 * j);
            uint64_t carry{0};
            for (size_t i = 0; i + j < words.size(); ++i) {
                // (2^32-1)^2 + 2*(2^32-1) fits uint64_t exactly.
                const uint64_t sum = uint64_t{words[i]} * digit + result[i + j] + carry;
                result[i + j] = sum;
                carry = sum >> 32;
            }
            assert(carry == 0);
            if (j != 0) assert(words.back() == 0 || digit == 0);
        }
        words = result;
    }

    void ShiftRight(unsigned shift)
    {
        const auto original = words;
        words.fill(0);
        if (shift >= 512) return;
        const size_t offset = shift / 32;
        const unsigned bits = shift % 32;
        for (size_t i = 0; i + offset < words.size(); ++i) {
            words[i] = original[i + offset] >> bits;
            if (bits != 0 && i + offset + 1 < words.size()) words[i] |= original[i + offset + 1] << (32 - bits);
        }
    }

    bool Fits256() const
    {
        for (size_t i = 8; i < words.size(); ++i) if (words[i] != 0) return false;
        return true;
    }

    arith_uint256 Low256() const
    {
        arith_uint256 result;
        for (int i = 7; i >= 0; --i) { result <<= 32; result += words[i]; }
        return result;
    }

    bool operator<=(const Uint512& other) const
    {
        for (int i = 15; i >= 0; --i) {
            if (words[i] != other.words[i]) return words[i] < other.words[i];
        }
        return true;
    }
};
} // namespace daa::detail

#endif // BITCOIN_DAA_MATH_H
