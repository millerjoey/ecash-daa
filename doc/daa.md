**Experimental eCash DAA: anchored**

Apply the common foundation and this alternative only. This build has one compiled algorithm. No public network has an activation height; a future eCash height requires a separate deployment patch and agreement. The optional height must be positive and strictly later than EcashHeight. The original height-973728 reset remains in the legacy calculation and contextual validation. A narrow plausibility exception admits its prescribed bits even outside the legacy fourfold envelope.

At H through H+143 carry H-1's bits, suppressing any intervening legacy retarget. Retarget at H+144 and then every 144 blocks. The first measurement spans H-1 through H+143; subsequent periods share their boundary block. Legacy target timespan, spacing, BIP9 periods, thresholds, timestamp validity, and public network identities are unchanged.

Use the target, raw timestamp, and height of H-1 on the candidate branch as the fixed anchor. The real-valued formula is Qa * exp(((t-ta)-600*(h-ha))/1209600). Do not use MTP or per-period clamps. Calculate the base-2 exponent in Q32 by truncating signed division of error*2^80 by (1209600*195103586505167). The latter constant is floor(ln(2)*2^48). Split into floor integer and nonnegative fractional parts; multiply the fractional part by that Q48 ln(2), floor to Q48, then evaluate eighteen Taylor terms for exp(x), flooring each term. Scale the original anchor target by this Q48 factor and the integer power of two in one widened operation. Saturating at error outside +/-256*1209600 is justified by absolute 256-bit representability. Headers presync and redownload each track their own branch anchor and reject targets that do not match the same function. The context-free plausibility interface alone admits every valid target at a new boundary.

Arithmetic saturates to [1,powLimit] before the existing compact encoding. Compact conversion rounds down again. Mathematical multiplier bounds and cancellation statements are subject to integer/fixed-point and compact quantization. Ratio/exponential approximation error is checked before compact encoding, separately allowing one target unit for the final floor. No floating-point computation is used in consensus.

`-regtest -test=daa` is the isolated development mode. It uses synthetic H=200 and the algorithm in this source patch. It retains normal regtest before H; after H it ignores regtest's no-retarget and special minimum-difficulty escapes. Ordinary regtest is unchanged. `-test` is rejected on other networks by the existing startup checks. Combining DAA test mode with BIP94 is rejected. No public-testnet activation or minimum-difficulty interaction is specified by this experiment.

Build and verify (replace python3 with a Python 3.10+ interpreter):

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTS=ON -DENABLE_IPC=OFF -DENABLE_WALLET=OFF
cmake --build build --target bitcoind bitcoin-cli test_bitcoin
build/bin/test_bitcoin --run_test=daa_common_tests,daa_tests,pow_tests,headers_sync_chainwork_tests
python3 contrib/daa_reference.py
python3 test/functional/feature_daa.py --configfile=build/test/config.ini
```

`python3 contrib/daa_reference.py --write-vectors` regenerates the C++ vector header. The script checks 2234 inputs against a 120-digit Decimal oracle, confirms compact stability at 170 digits, and runs 126 retargets under seeded constant, step, and oscillating hashrate profiles. C++ checks exact bit outcomes, activation at and between old boundaries, raw time reversals, limits, extreme exponents, header admission, and both headers-sync phases. The functional test mines four retarget periods, checks template/header/block bits, synchronizes from genesis, verifies ordinary regtest, replaces the activation anchor in a reorganization, and restarts.

For unchanged Core functional regressions on this eCash base, run `python3 contrib/ecash_functional.py mining_basic.py --configfile=build/test/config.ini` (substitute other scripts as needed). This test-only adapter supplies the existing eCash magic and configuration filename and enables the base fork's documented deprecated mining RPC escape for tests. It changes no node consensus or network identity and avoids modifying upstream test files.

MTP manipulation, clipping, and recursive path dependence remain concerns for the partial retargets. Anchored exponential avoids recursive estimator drift but retains raw timestamp influence and an anchor that can differ on a reorganization. Simulations are behavioral checks, not stability or adversarial security proofs. Deploying this code remains a separate consensus decision.
