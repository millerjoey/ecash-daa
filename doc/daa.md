**Experimental eCash DAA: ASERT (aserti3-2d)**

Apply the unchanged common foundation plus `0002-daa-asert.patch`. This is the fourth independent alternative. Each source patch supplies one compiled algorithm; every public `DAAHeight` remains unset. The production algorithm and future eCash activation height are still deployment decisions.

This option follows [BCHN's aserti3-2d arithmetic specification](https://upgradespecs.bitcoincashnode.org/2020-11-15-asert/): 600-second spacing, a 172800-second base-2 half-life, raw timestamps, Q16 exponent quantization, the published cubic approximation, and no relative adjustment clamp. The absolute `powLimit` and minimum representable target 1 still apply. This is a different response speed as well as a different cadence from the original 144-block anchored exponential alternative, whose natural-exponential time constant is fourteen days.

The eCash integration preserves H as a carry block and starts per-block adjustments at H+1. For candidate height n > H, the fixed target/height anchor is H−1 and the timestamp reference is its parent, H−2. Using evaluation block h=n−1, the ideal formula is:

```text
Q(n) = Q(H−1) * 2^(((t(h) − t(H−2)) − 600 * (h − (H−1) + 1)) / 172800)
```

The arithmetic includes the standard `height_delta + 1` convention. The anchor's own raw timestamp is not the timestamp reference. H must be at least 2 and later than `EcashHeight`. The public future-fork requirement already places it beyond the original height-973728 reset. The original retarget function and separate eCash reset enforcement remain intact.

`daa::Retarget()` in this alternative returns true for every height after H. This changes the new schedule only; legacy target timespan, BIP9 periods/thresholds, and timestamp validity retain their original settings. The initial carry block and height-based deployment are eCash integration choices; BCH's activation mechanism and public-testnet policies are not introduced.

Signed exponent division truncates toward zero. The integer/fractional split then uses floor division with a nonnegative Q16 fraction. The cubic coefficients and rounding constant are exactly those specified for aserti3-2d. The full target product is widened before the final power-of-two shift, so regtest's 255-bit limit is supported safely. Extreme errors beyond ±256 half-lives saturate by absolute 256-bit representability, without adding a per-block clamp. All consensus operations use standard integer types and the existing common helper.

The target is always derived from the candidate branch's original anchor. Headers presync and redownload each track their own anchor and anchor-parent timestamp, initialized either from known ancestry or the header stream. Both phases verify exact ASERT targets on every active adjustment. The context-free transition check admits valid targets at these heights; the stateful check supplies the missing anchor/time context. There is no global anchor cache to survive a reorganization incorrectly.

`-regtest -test=daa` activates this source patch at synthetic H=200. Standard regtest is unchanged. After activation the test mode ignores its normal no-retargeting and minimum-difficulty escapes. Existing startup checks reject `-test` outside regtest, and combining this mode with BIP94 is rejected. No network identity or public-testnet activation setting changes.

Build and test with Python 3.10 or later:

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTS=ON -DENABLE_IPC=OFF -DENABLE_WALLET=OFF
cmake --build build --target bitcoind bitcoin-cli test_bitcoin
build/bin/test_bitcoin --run_test=daa_common_tests,daa_tests,pow_tests,headers_sync_chainwork_tests
python3 contrib/daa_reference.py
python3 test/functional/feature_daa.py --configfile=build/test/config.ini
```

`contrib/daa_reference.py --write-vectors` regenerates both C++ headers. Tests include 234 numerical edge vectors and 396 expected outcomes selected directly from the [twelve published ASERT runs](https://download.bitcoincashnode.org/misc/data/asert/test_vectors/). The independent integer reference was also checked against all 14000 published outcomes; `--upstream-dir /path/to/runs` reproduces that larger check. Provenance and file hashes are in `src/test/data/asert_vectors.README.md`.

The reference also checks 2234 cases against 120-digit Decimal arithmetic with 170-digit compact stability, all 65536 polynomial fractions for monotonicity and unsigned bounds, and 18144 per-block adjustments under seeded constant, step, and oscillating hashrate. Its numerical tolerance is 1.5e-4 relative before compact conversion, allowing the specified ASERT cubic and exponent quantization, plus one target unit for flooring. The original higher-precision anchored alternative retains its tighter 5e-10 tolerance.

C++ and node tests cover initial carry, first adjustment, consecutive block adjustments, exact half-life doubling/halving, raw time reversal, limits and extreme errors, template/header/block bits, both headers-sync phases, standard regtest, restart, and a reorganization replacing the anchor and its parent. Existing Core tests can use `contrib/ecash_functional.py` for this base's established configuration filename, magic bytes, and mining-RPC testing policy.

Raw timestamp and anchor-parent influence remain relevant. Numerical agreement and these chain scenarios do not establish a general stability or adversarial security proof. Public deployment remains separate from this experiment.
