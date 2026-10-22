**Four alternative eCash DAA patch series**

Base: `ecash-com/bitcoin` commit `6e582335d1210af7963f20ceff371edd37c79af5` (Core v31.1 plus the existing eCash patches).

Apply `0001-daa-common.patch`, followed by exactly one of:

- `0002-daa-ratio.patch`: geometric correction with exact integer evaluation of a downward Q32 fourteenth-root multiplier; MTP; 21600–345600-second clamp.
- `0002-daa-anchored.patch`: fixed-anchor exponential correction; raw timestamps; no relative clamp; exact anchored checks in both headers-sync phases.
- `0002-daa-linear.patch`: exact rational partial correction; MTP; 0–212306-second clamp.
- `0002-daa-asert.patch`: BCH-style aserti3-2d; per-block adjustments; two-day base-2 half-life; anchor-parent raw timestamps; standard Q16 cubic rounding; no relative clamp.

For example, from a clean checkout of the pinned base:

```sh
git apply /path/to/patches/0001-daa-common.patch
git apply /path/to/patches/0002-daa-ratio.patch
```

The supplied `../bitcoin/` working tree already contains the common foundation plus the linearized alternative. Use a clean base checkout to apply a different alternative. These are mutually exclusive source patches, with one compiled algorithm per build.

Every public activation height remains unset. `-regtest -test=daa` enables only the selected build's algorithm at synthetic test height 200. Ordinary regtest retains its existing behavior. The eventual public algorithm and future eCash activation height require a separate deployment decision.

After applying a series, `doc/daa.md` documents its precise arithmetic, activation schedule, testing commands, and limitations. `contrib/daa_reference.py` verifies the integer specification against a high-precision reference and regenerates the C++ numerical vectors. `feature_daa.py` covers mining, validation, synchronization, restart, and reorganizations.

See `../DAA_DESIGN.md` for the architecture and `../IMPLEMENTATION.md` for measured diffs and completed checks. All four series were verified to apply cleanly to the pinned base.

The original common, ratio, anchored, and linear patch files retain their previous checksums. ASERT is an additional alternative; it starts adjustments at H+1 after one carry block, while the original three first adjust at H+144. It changes both cadence and response speed relative to the anchored exponential experiment, so comparison results should account for both.

| Alternative | Retarget cadence | Timestamp reference | Response parameter | First adjustment |
|---|---:|---|---|---|
| Ratio | 144 blocks | Shared-boundary MTP | Geometric correction 1/14 | H+144 |
| Anchored exponential | 144 blocks | Raw H−1 anchor time | Natural-exponential time constant 14 days | H+144 |
| Linearized | 144 blocks | Shared-boundary MTP | Arithmetic correction 1/14 | H+144 |
| ASERT | Every block | Raw H−2 anchor-parent time | Base-2 half-life 2 days | H+1 |
