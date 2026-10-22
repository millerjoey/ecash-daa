**eCash DAA source review and proposed implementation**

Status: implemented as a common patch and four mutually exclusive alternative patches. Agreed scope: a future fork of the current eCash chain; unit tests and isolated regtest only; the proposed ratio and linear clamps. Public activation remains unset and network identities are unchanged. See IMPLEMENTATION.md for validation and patch instructions.

The workspace was empty, so this review used the GitHub connector to inspect `ecash-com/bitcoin`, rather than substituting Bitcoin Core sources. The reviewed default-branch tip is [6e582335d1210af7963f20ceff371edd37c79af5](https://github.com/ecash-com/bitcoin/commit/6e582335d1210af7963f20ceff371edd37c79af5), verified through the `master` branch API. The repository's `v31.1` tag resolves to `9be056a8a72b624dae9623b2f7bded92c2a21c91`. Their [comparison](https://github.com/ecash-com/bitcoin/compare/v31.1...6e582335d1210af7963f20ceff371edd37c79af5) contains 12 subsequent commits and 26 changed files. This is a review baseline, not a deployment decision.

**Existing fork structure**

The patch series covers data/configuration paths, network identity and seeds, replay protection, the difficulty reset, drivechain-related script and transaction support, mining RPC changes, branding, packaging, and a snapshot update. These existing changes remain the base for all three alternatives.

The complete difference between `v31.1` and the reviewed [pow.cpp](https://github.com/ecash-com/bitcoin/blob/6e582335d1210af7963f20ceff371edd37c79af5/src/pow.cpp#L48) is the four-line addition that sets `EcashForkBits` when the next height equals `EcashHeight`, inside `CalculateNextWorkRequired()`. This exact comparison was checked programmatically. Mainnet [chain parameters](https://github.com/ecash-com/bitcoin/blob/6e582335d1210af7963f20ceff371edd37c79af5/src/kernel/chainparams.cpp#L82) set height 973728 and bits `0x1901a6e1`. That height is a 2016-block boundary. Mainnet retains 600-second spacing and a 1209600-second legacy target timespan.

There is a second enforcement point: [ContextualCheckBlockHeader()](https://github.com/ecash-com/bitcoin/blob/6e582335d1210af7963f20ceff371edd37c79af5/src/validation.cpp#L4133) separately requires the reset bits at the fork height after checking `GetNextWorkRequired()`. Both checks must be preserved. An early DAA dispatch that simply returns the previous block's bits at the original fork would conflict with this enforcement.

Testnet4 retains its minimum-difficulty exception, BIP94's use of the first block's target when retargeting, and the additional timestamp check at its existing interval boundaries. Regtest retains a nominal 144-block interval and `fPowNoRetargeting = true`. BIP9 deployments have their own explicit periods and thresholds; these must not follow the new DAA interval.

`PermittedDifficultyTransition()` is currently unchanged from Core. It knows only the legacy schedule and fourfold bounds, and returns early on networks allowing minimum difficulty. Both [headers presync phases](https://github.com/ecash-com/bitcoin/blob/6e582335d1210af7963f20ceff371edd37c79af5/src/headerssync.cpp#L177) call it. It also has no explicit eCash reset exception: a reset outside its legacy bounds could fail presync even though full consensus validation requires that reset. Add a focused regression and, if needed, a narrowly scoped allowance for the prescribed reset bits; do not change historical block requirements.

**Proposed patch structure**

Use one independently buildable foundation followed by three alternative implementation branches or patch series, each based on that foundation. Each alternative provides one compiled algorithm. There is no production algorithm selector.

The foundation adds an optional activation height, disabled by default, separate from `EcashHeight`; constants for 600 seconds, 144 blocks, 86400 seconds, and smoothing 14; schedule/boundary helpers; and tests. It need not dispatch to an unimplemented algorithm. Each alternative then adds its selected implementation and the small dispatch hooks.

| File or interface | Proposed change |
|---|---|
| `src/consensus/params.h` | One optional DAA activation height, initially unset on every network. |
| New `src/daa.h` and `src/daa.cpp` | Isolated schedule, boundary lookup, target arithmetic, and selected algorithm. Split a mathematical helper into another file only if that improves review. |
| `src/pow.cpp` | Small activation dispatch in `GetNextWorkRequired()` and `PermittedDifficultyTransition()`. Keep the legacy calculation, including the eCash reset, intact. |
| `src/pow.h` | Update the plausibility-check comment; retain public function signatures. |
| `src/CMakeLists.txt`, `src/kernel/CMakeLists.txt` | Register new sources in both libraries that currently compile `pow.cpp`. |
| `src/test/daa_tests.cpp`, `src/test/CMakeLists.txt`, `src/test/pow_tests.cpp` | New numerical/schedule tests and focused history/reset regressions. |
| `src/kernel/chainparams.h`, `src/kernel/chainparams.cpp`, `src/chainparams.cpp`, test-option documentation | Regtest-only development hook if approved. Public activation remains unset. |
| New `test/functional/feature_daa.py`, `test/functional/test_runner.py` | Isolated integration tests and registration. |
| `src/headerssync.h`, `src/headerssync.cpp` for anchored exponential | Small per-sync anchor state and exact anchored-target checks, described below. |

`src/validation.cpp`, timestamp validity, versionbits, chain selection, network identity, and sidechain interfaces need no new consensus changes under the recommended initial scope. Existing eCash validation remains in place.

**Activation and indexing**

Let `n` be the candidate block height. Before `H`, execute the existing code unchanged. At `H` through `H+143`, carry the preceding regime's final validated target. For an activation strictly after the reset, the proposed interpretation is the target in block `H−1`. If `H` is an ordinary legacy retarget boundary, this deliberately suppresses that ordinary retarget; otherwise the first period could inherit a different target from the fixed anchor. This interpretation is explicit for review.

The new retarget condition is `n > H && (n − H) % 144 == 0`. At a retarget, the completed boundary is `n−1`, and the previous shared boundary is `n−145`:

| Candidate | Start boundary | Completed boundary | Height difference |
|---|---|---|---|
| `H+144` | `H−1` | `H+143` | 144 |
| `H+288` | `H+143` | `H+287` | 144 |

Ratio and linearized calculations use MTP at these boundary indices. Anchored exponential always uses height, raw timestamp, and target from the branch's `H−1` block, plus the latest completed boundary's raw timestamp and height. It never compounds a previously rounded target. Retrieve the anchor from the candidate's ancestry so reorganizations across activation or the anchor cannot reuse another branch's values. Require `H >= 1`.

Choosing `H = 973728`, or enabling the new regime before that height on a chain retaining eCash history, needs a separate transition decision. The required reset target need not equal the `H−1` target. Silently carrying the old target would bypass the reset; silently replacing the anchor's target would change the requested anchored formula. No overlapping deployment is proposed without agreement.

**Arithmetic and clamp review**

Ratio uses deterministic integer/fixed-point fractional-power arithmetic over the proposed inclusive span range `[21600, 345600]`. Linearized uses the exact rational numerator `1123200 + s`, denominator `1209600`, and proposed inclusive span range `[0, 212306]`. Round once after the full product/division, then encode compactly. Specify fixed-point precision, constants, signed division, rounding, and approximation bounds as consensus behavior before finalizing numerical vectors.

At 90 decimal digits, the ideal maximum multiplier is approximately `1.10408951367381233765`. Solving the linear bound gives `212306.67573984340362` seconds. Thus 212306 is the largest whole-second upper clamp satisfying the proposed cap; 212307 exceeds it. Fourteen maximum linear steps multiply the ideal target by approximately `3.999971665205826`, before compact rounding and the absolute limit. A zero lower clamp leaves the ideal minimum multiplier at `13/14`; raising it would reduce the maximum difficulty increase and alter recovery from fast blocks.

MTP at shared boundaries removes the omitted-interval measurement and reduces sensitivity to a single timestamp. It does not eliminate coordinated timestamp manipulation. Median windows can be influenced, clipping discards part of a timing error, and nonlinear recursive updates do not exactly cancel opposing errors. Retargeting only every 144 blocks also holds the target fixed during within-period hashrate changes. These are concerns to test, not grounds to alter timestamp validity or claim a security proof.

Anchored exponential uses the requested natural-exponential denominator of 1209600 seconds, with no relative clamp. Its equivalent base-2 half-life is approximately `838430.82960530984627` seconds. Do not substitute 1209600 as a base-2 half-life or silently round that equivalent half-life to whole days. If reusing the [BCH ASERT approximation](https://upgradespecs.bitcoincashnode.org/2020-11-15-asert/), specify a deterministic base conversion and adapt its parent-of-anchor timestamp and `height_delta + 1` conventions: they differ from this request. BCH vectors can check reused arithmetic but cannot be copied as vectors for the new anchor convention and time constant.

All algorithms need nonzero target saturation and the existing absolute `powLimit` and compact encoding. Compact quantization means ideal multiplier bounds and mathematical cancellation are not promises of exact encoded results. Extreme positive and negative schedule errors must saturate safely rather than overflow or return zero.

Direct `arith_uint256` multiplication before division can overflow at regtest's approximately 255-bit limit, even when the final rational result is representable. The existing legacy sanity check explicitly skips no-retargeting regtest. Use exact quotient/remainder arithmetic or an isolated widened helper with explicit saturation; do not change the common integer class or depend on multiplication wrapping. The same issue applies to fixed-point multipliers.

**Headers synchronization and development networks**

For ratio and linearized, use the activation-relative schedule and compute plausibility bounds through the same arithmetic and compact rounding as full validation. Between retargets, require unchanged bits. Handle the carry period and original reset explicitly.

For anchored exponential, a context-free check with only height and adjacent bits cannot validate the anchor formula. At new retarget boundaries it can require a valid nonzero target within `powLimit`; between them it can require equality. Retaining the fourfold bound would reject some valid anchored chains. Merely relaxing it also weakens an existing presync defense, so the proposed implementation additionally checks the exact anchored target in both presync phases using small branch-specific anchor state and the previous header. Initialize that state from the known fork point's ancestry, or capture `H−1` as the stream crosses it. Keep separate phase state and retain PoW checks, commitments, resource bounds, and later full contextual validation. This avoids imposing an invented consensus clamp.

Recommended initial scope is unit tests plus an explicitly enabled isolated regtest mode. Standard regtest and all public networks retain their present behavior. The test mode enables this branch's compiled DAA only in its active testing regime, with no special minimum-difficulty escape there; it preserves legacy regtest behavior before activation. Use the existing regtest-only test-option framework, which rejects `-test` on other networks. No production runtime algorithm selection is added.

Do not activate on Testnet4 initially. If it is requested, first specify its interaction with minimum-difficulty recovery, BIP94 target selection, and BIP94's existing timestamp boundaries. Do not infer permission to change timestamp validity from a DAA interval change. A separately operating experimental chain would also need agreed message bytes, ports, seeds, chain identity, and any genesis/history policy; none are selected here.

**Verification and maintenance**

Each alternative will include exact-bit pre-activation regressions; reset preservation through full work calculation and header admission; activation at and between legacy boundaries; the first two 144-interval measurements; valid nonmonotonic raw timestamps; median manipulation; clamp endpoints and one-second neighbors; zero spans; huge schedule errors; nonzero underflow; absolute limits; overflow; and compact rounding. Anchored tests must distinguish different paths ending at the same anchor, height, and timestamp, and reorganizations that replace the anchor itself.

Use an independent high-precision reference and increasing precision where compact boundaries are close. Separately test exact fixed-point vectors and approximation error against the real-valued formulas. Seeded constant, step-change, and oscillating hashrate scenarios are behavioral tests, not security proofs. Functional coverage will include mining/template bits, rejection of wrong bits, headers presync/redownload, restart, and reorganizations. Run existing PoW, headers-sync, mining, block-validation, and relevant BIP94 tests on standard regtest as regressions. The source review initially checked the comparison and clamp calculation. Completed node builds, numerical checks, and unit/functional results are recorded in IMPLEMENTATION.md.

Expected rebase surface: linearized is smallest; ratio adds a fractional-power helper; anchored exponential adds exponential arithmetic, anchor handling, and the additional headers-sync integration. Most changes live in new files. Existing `CalculateNextWorkRequired()` and its eCash patch remain intact. Exact diff sizes and completed test results are recorded in IMPLEMENTATION.md.

The user approved a future fork of current eCash, unit tests plus isolated regtest, and the proposed clamps. A future public activation height and the eventual production algorithm remain to be agreed; every public network stays disabled in these patches.

**Fourth alternative: standard ASERT arithmetic**

At the user's subsequent request, `0002-daa-asert.patch` adds aserti3-2d with a two-day half-life and per-block evaluation. It shares the unchanged foundation, overrides the new retarget helper to adjust every block after H, and uses H−1's target with H−2's raw timestamp and the standard extra interval. H itself remains the carry block. All public activations remain unset. The original three patch files retain their checksums. The comparison must distinguish the changed cadence and shorter response time; ASERT also uses the standard Q16 cubic approximation while the original anchored option retains its Q32/Q48 arithmetic. See IMPLEMENTATION.md and the updated patch index for results and precise conventions.
