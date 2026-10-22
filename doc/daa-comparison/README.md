# Comparing the four eCash difficulty algorithms

All alternatives descend from the same pinned eCash commit,
`6e582335d1210af7963f20ceff371edd37c79af5`, through `daa/common`.
Each algorithm branch adds one implementation commit to that shared foundation.
The common branch is an integration foundation; use an algorithm branch to build.

| Branch | Cadence | Timestamp reference | Response | First adjustment |
|---|---|---|---|---|
| [daa/ratio](https://github.com/millerjoey/ecash-daa/tree/daa/ratio) | 144 blocks | Shared-boundary MTP | Geometric correction 1/14 | H+144 |
| [daa/anchored](https://github.com/millerjoey/ecash-daa/tree/daa/anchored) | 144 blocks | Raw H−1 anchor | Natural-exponential time constant 14 days | H+144 |
| [daa/linear](https://github.com/millerjoey/ecash-daa/tree/daa/linear) | 144 blocks | Shared-boundary MTP | Arithmetic correction 1/14 | H+144 |
| [daa/asert](https://github.com/millerjoey/ecash-daa/tree/daa/asert) | Every block | Raw H−2 anchor-parent | Base-2 half-life 2 days | H+1 |

ASERT changes both cadence and response speed relative to the anchored experiment.
Ratio clamps the measured span to 21600–345600 seconds; linear clamps it to
0–212306 seconds. The anchored and ASERT alternatives have no relative clamp.

Every public activation height remains unset. The only enabled development path
is unit tests and isolated `-regtest -test=daa`, using synthetic H=200.
Ordinary regtest keeps its existing behavior. This repository does not select a
production algorithm or future public activation height.

## Review an algorithm against the common foundation

- [ratio](https://github.com/millerjoey/ecash-daa/compare/daa/common...daa/ratio)
- [anchored](https://github.com/millerjoey/ecash-daa/compare/daa/common...daa/anchored)
- [linear](https://github.com/millerjoey/ecash-daa/compare/daa/common...daa/linear)
- [asert](https://github.com/millerjoey/ecash-daa/compare/daa/common...daa/asert)

For a direct comparison, choose two algorithm branches. For example:
[linear versus ASERT](https://github.com/millerjoey/ecash-daa/compare/daa/linear..daa/asert).
GitHub's three-dot comparison shows changes since the common ancestor;
use the two-dot URL above or local `git diff` to compare the two branch tips.

```sh
git clone https://github.com/millerjoey/ecash-daa.git
cd ecash-daa
git switch --track origin/daa/linear
git diff origin/daa/common..origin/daa/linear
git diff origin/daa/linear..origin/daa/asert
```

Each algorithm's [doc/daa.md](../daa.md) specifies its arithmetic, schedule,
testing commands, and limitations. `contrib/daa_reference.py` checks its integer
arithmetic against a high-precision reference. `feature_daa.py` exercises mining,
validation, synchronization, restarts, and reorganizations.

The archived [design report](DAA_DESIGN.md), [implementation report](IMPLEMENTATION.md),
and [portable patches](patches/README.md) describe the original patch delivery.
References there to the local `bitcoin/` checkout describe that earlier workspace.
Apply the common patch and exactly one alternative to the pinned base; do not
apply them on top of these branches, where they are already incorporated.

All four variants previously passed their C++ DAA/PoW/headers suites,
high-precision numerical checks, and isolated regtest functional tests. ASERT
also matched all 14,000 published BCH test-vector rows; 396 sampled published
vectors are included in its C++ tests. Those results predate this conversion
to branches; the source and portable patch bytes are preserved unchanged.
