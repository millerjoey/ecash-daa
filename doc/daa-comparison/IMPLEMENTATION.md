**Implemented eCash DAA alternatives**

The deliverables are one common foundation and four mutually exclusive implementation patches in `patches/`. The local `bitcoin/` checkout contains the common foundation and linearized implementation. Ratio, anchored exponential, and ASERT are separate source patches, with their own reference module, numerical vectors, documentation, and tests. No production runtime algorithm selector was introduced.

The agreed scope is a future fork of the current eCash chain, development through unit tests and isolated regtest, and the proposed clamps. Every public network's `DAAHeight` remains unset. No future public height was chosen. Network identity, timestamp validity, BIP9 periods/thresholds, and sidechain functionality remain unchanged.

The pinned eCash base is `6e582335d1210af7963f20ceff371edd37c79af5`. Its existing `CalculateNextWorkRequired()` function, including the height-973728 reset, is byte-identical in the implementation. The separate reset enforcement in `validation.cpp` is untouched. The common patch adds a narrow headers-plausibility exception for the prescribed reset bits, because the legacy fourfold check could otherwise reject that required transition.

For the original three alternatives, activation carries block H−1's validated target through H+143. The first retarget is H+144, measuring boundaries H−1 and H+143, followed by shared boundaries every 144 blocks. A positive public height must be strictly later than `EcashHeight`. Synthetic H=200 exercises activation between old regtest boundaries. `-test=daa` is rejected outside regtest, and combining this test mode with BIP94 is rejected. Standard regtest remains unchanged; the active testing regime ignores its normal no-retargeting and minimum-difficulty escapes.

ASERT retains only H as a carry block, then adjusts every block from H+1 with a two-day base-2 half-life. It uses target/height from H−1, raw timestamp from H−2, and the standard `height_delta + 1` convention. This is height-gated eCash integration with BCH aserti3-2d arithmetic. The original 144-block anchored experiment retains its fourteen-day natural-exponential time constant.

The common foundation provides overflow-safe exact rational scaling, bounded widened intermediates, schedule helpers, and an optional activation field. It changes no common integer implementation or existing public function signature. Ratio computes an exact integer inequality for a downward Q32 fourteenth-root multiplier. Linearized applies the full rational product before division. Anchored exponential uses a fixed H−1 anchor, Q32 exponent conversion, and an eighteen-term Q48 exponential series, with explicitly specified rounding and absolute saturation. Its per-phase headers-sync state checks the same target formula without imposing an invented relative clamp.

| Patch | Changed files | Added / removed lines | Non-test C++ added / removed |
|---|---:|---:|---:|
| Common foundation | 10 | +268 / −0 | +176 / −0 |
| Ratio, above common | 18 | +820 / −3 | +109 / −3 |
| Anchored, above common | 21 | +898 / −4 | +187 / −4 |
| Linearized, above common | 18 | +799 / −3 | +88 / −3 |
| ASERT, above common | 26 | +1746 / −7 | +180 / −6 |

Most alternative-patch lines are new tests, reference arithmetic, vectors, and documentation. Linearized has the smallest maintenance surface. Ratio adds the root calculation in its isolated algorithm file. Both exponential options integrate branch-specific anchor state into two existing headers-sync files. ASERT also specializes the new retarget helper for a per-block schedule; its extra data files preserve published expected outputs. The legacy retarget body is preserved across all four, limiting future merge conflicts to small dispatch, parameter, build-registration, and test-option additions.

Completed validation on macOS arm64 with AppleClang 21, C++20, and a Debug build:

- Built `bitcoind`, `bitcoin-cli`, and `test_bitcoin`; built each alternative separately. Also built the final `bitcoinkernel` library target.
- The original three alternatives passed `daa_common_tests`, `daa_tests`, `pow_tests`, and `headers_sync_chainwork_tests`: 25 test cases per build, including 234 exact numerical vectors per algorithm.
- The original three alternatives passed `feature_daa.py`: four retargets with constant, abrupt, and oscillating timings; valid raw time reversal; template/header/block difficulty checks; headers synchronization; standard-regtest preservation; a reorganization replacing the H−1 anchor; and restart.
- The original three alternatives passed 2234 comparisons against a 120-digit Decimal oracle, with compact results checked again at 170 digits, plus 126 retargets in seeded constant, step, and oscillating hashrate simulations. Partial-retarget simulations use shared-boundary MTP.
- ASERT passed the DAA/PoW/headers suites plus the mining/block/versionbits unit suites: 33 test cases total, including 234 numerical edge vectors and 396 expected results selected from the published ASERT runs. The ASERT node test passed per-block template/header/block checks, synchronization, standard regtest, restart, and a reorganization replacing the anchor and its parent.
- The ASERT integer reference matched all 14000 published outcomes. It also passed 2234 Decimal comparisons, all 65536 polynomial fractions for monotonicity/unsigned bounds, and 18144 simulated per-block adjustments. Its pre-compact tolerance is 1.5e-4 relative plus one target unit, reflecting the standard Q16 cubic; the original high-precision exponential helper remains at 5e-10.
- Existing `miner_tests`, `validation_block_tests`, and `versionbits_tests` passed: six additional unit test cases.
- Existing `mining_basic.py`, `feature_block.py`, and `p2p_headers_sync_with_minchainwork.py` passed, including BIP94 timewarp checks and long reorganizations.
- Both startup guard checks passed: mainnet rejects `-test=daa`; regtest rejects DAA test mode combined with BIP94.
- All four final patch series applied cleanly to fresh archives of the pinned eCash base. Applied files matched the tested variant manifests, and the high-precision reference checks passed from each clean application.
- `git diff --check` and Python syntax checks passed.

The unchanged Core functional tests need a small eCash adapter because this base retains their Bitcoin configuration filename, wire magic, and mining-RPC assumptions. `contrib/ecash_functional.py` supplies the existing eCash values and the base fork's documented deprecated mining-RPC testing escape, without editing upstream tests or changing node consensus.

The common and alternative patches include their own tests and reproducible commands in `doc/daa.md`. Build/test logs are retained under `validation/`. The architecture and timestamp/clamp discussion remain in `DAA_DESIGN.md`.

Public deployment, cross-platform release validation, and an adversarial security proof are outside this experiment. MTP influence and clipping/path dependence remain relevant for the partial retargets; raw timestamp and anchor manipulation remain relevant for anchored exponential. The numerical and chain tests do not establish a general stability or security result.

The added ASERT variant follows the [BCHN arithmetic specification](https://upgradespecs.bitcoincashnode.org/2020-11-15-asert/). Its offline vector subset includes source links and hashes; the full twelve-run download was checked separately. The common foundation and original three patch files remain byte-for-byte unchanged. The working checkout remains linearized after validation of the fourth build. ASERT's shorter half-life and per-block cadence both affect a comparison with the fourteen-day anchored experiment.
