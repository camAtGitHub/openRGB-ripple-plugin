# Implementation plan: studio modes → lockstep plugin

Execute **one phase per session**. Copy from the cited files; do not invent fields, Qt widgets, or test runners. Studio on **8085** (`AGENTS.md`). **No Playwright** — user inspects the page.

The web studio already contains the prototype. This plan **pins that behavior**, then **copies it into** `OpenRGBRipplePlugin/RippleEngine.h` + `RippleWidget`. Bolt / C1 / C2 stay later (see `docs/superpowers/specs/2026-08-21-jump-modes-design.md`).

---

## Phase 0 — Allowed APIs (discovery)

### Studio source of truth

| API | Location |
|---|---|
| `WaveShape` `"circle" \| "square" \| "axis" \| "sweep" \| "jump"` | `src/lib/ripple.ts:4-5` |
| `RippleSettings` (incl. `fade`, `axisJitter`, `sweepSpan`, `trailLength`, `blastSize`, `blastShape`) | `src/lib/ripple.ts:62-111` |
| `spawnRipples(settings, x, y, now, seed, bounds?, from?)` | `src/lib/ripple.ts:255-316` |
| `spawnBlast(settings, x, y, now, seed, fromDart?)` | `src/lib/ripple.ts:318-353` |
| `waveRadius(speed, elapsed, expand, fade, maxRadius?)` | `src/lib/ripple.ts:440-457` |
| `pickAxisDirection(x, y, bounds, seed, jitter)` | `src/lib/ripple.ts:157-195` |
| `rainbowAtDistance(dist, hueOffset?)` | `src/lib/ripple.ts:232-239` (`RAINBOW_KEYS_PER_CYCLE = 8`) |
| Dart idle blast + wipe | `src/components/keyboard-stage.tsx:127-208` |
| Controls (shape / jitter / span / trail / blast) | `src/components/controls.tsx:129-337` |

**Behavior locked in studio (copy these rules, not older comments):**

- **Fade** = seconds to **retract** the front to the origin. `0` = snap off. Not `pow(1-t, fade_power)`.
- **Lifetime** (circle/square) = expand seconds. Peak radius = `speed * lifetime` unless `maxRadius` is set.
- **Row/Col + Sweep** = `pickAxisDirection`; outward trip lasts **exactly `lifetime`** (`maxRadius = travel`, speed = `travel/lifetime`). Jitter = chance of the short way. Sweep lateral = `spanLat`.
- **Fill** = hard front `dist <= radius ? 1 : 0` (not `1 - dist/radius`).
- **Square** = Chebyshev `max(|dx|,|dy|)`.
- **Rainbow** = hue from **wave distance** (`dist / 8`), not a random colour per press.
- **Dart (`jump`)** = takeoff `from` → landing press. Flight = `hypot / speed`. Trail behind head. **One** blast after **idle `lifetime` from last keydown**. Blast colour = last dart arrival colour (`dartArrivalColor`). Blast **wipes darts**. Same-key / first press: `len < 0.2` lights only that key (never the whole board).
- Echoes already spawn extra darts; `echoDelay` has **no slider**.

### Plugin APIs that exist (copy these; do not invent)

| API | Location |
|---|---|
| `RippleSettings` (`fade_power` **is a power**, no `shape`) | `OpenRGBRipplePlugin/RippleEngine.h:54-72` |
| `SampleUnlocked` Euclidean circle + `pow` fade | `RippleEngine.h` ~332–407 |
| Widget sliders Speed/Thickness/Lifetime/Fade/Echoes/Brightness | `RippleWidget.cpp:236-241` mapping `370-375` |
| JSON persist | `RippleSettingsIO.h` |
| Hostless tests | `OpenRGBRipplePlugin/tests/hostless/test_ripple_engine.cpp` + `tests/CMakeLists.txt` (`check` target) |
| Version | `OpenRGBRipplePlugin.pro` `VERSION = 1.0.6` |

Widget Qt types already used: `QCheckBox`, `QComboBox`, `QColorDialog`, `QGridLayout`, `QGroupBox`, `QLabel`, `QPushButton`, `QSlider`, `QTimer`, `QVBoxLayout`. Do not add other Qt classes unless already in that file.

Plugin-only keep: `enabled`, `paint_idle`. JSON already has `paint_idle`; it does **not** persist `impact_hold`.

### Anti-patterns (Phase 0)

- Do **not** treat `fade_power` as retract seconds without renaming / dual-mapping.
- Do **not** invent `RippleEngine.cpp` (header-only). TS comment saying `.cpp` is stale.
- Do **not** use Playwright (`AGENTS.md`).
- Do **not** load the Windows DLL on this Linux host as “tests passed.”
- Do **not** copy Bresenham: studio Dart is **Euclidean projection** (`jumpCoverage` in `ripple.ts:504-535`), not key-index DDA. Spec vs code: **code wins**.
- Do **not** implement Bolt / C1 / C2 in Dart phases.
- SDK client `public/sdk-client/RippleEngine.h` is a **stale snapshot** (Over/Add only). Canonical client build is `OpenRGBRipplePlugin/sdk-client/` including `../RippleEngine.h`.

---

## Phase 1 — Pin studio with hostless-style checks

**What:** Add a small Node runner that imports `src/lib/ripple.ts` (same as the ad-hoc `--experimental-strip-types` checks used while prototyping). Copy the C++ `Expect(bool, const char*)` + `g_fails` pattern from `OpenRGBRipplePlugin/tests/hostless/test_ripple_engine.cpp:1-30`.

**Copy from:**

- Circle vs square distance: Chebyshev corner on, Euclidean corner idle — same idea as existing ring tests in `test_ripple_engine.cpp:87+`.
- `waveRadius` expand/retract: `ripple.ts:440-457`.
- Dart same-key: `jumpCoverage` `len < 0.2` (`ripple.ts:500-507`).
- `dartArrivalColor` (`ripple.ts` after `spawnRipples`).
- Sweep/axis: `pickAxisDirection` jitter 0 → np0 left-or-up only (`ripple.ts:157-195`).

**Put tests in:** `src/lib/ripple.check.ts` (or `OpenRGBRipplePlugin/tests` is C++ only — do **not** drop TS into that CMake target). Run: `node --experimental-strip-types src/lib/ripple.check.ts`. There is **no** `npm test` script today (`package.json` only `dev`/`build`/`preview`) — add `"test:ripple": "node --experimental-strip-types src/lib/ripple.check.ts"` by copying the scripts block style in `package.json`.

**Cases (must fail if someone reverts today’s rules):**

1. Square fill, fade 0: Chebyshev inside on, 0.1 outside idle; corner on; circle near-edge still wash **only if** you have not yet ported hard fill to circle — **current studio hard-fills all fills** (`sampleRipples` `brush === "fill"`). Copy that: circle fill is hard too.
2. `waveRadius(10, 2.5, 1, 3)` → 5 (retract). `waveRadius(10, 1.01, 1, 0)` → `null`.
3. Directed: `waveRadius(14, 0.35, 0.35, 0, 14)` → 14 (lifetime 0.35 crosses in 0.35s, not ~1s).
4. Same-key dart does not light a point 10 units away.
5. Rainbow: `rainbowAtDistance(0)` red-ish; `rainbowAtDistance(4)` cyan-ish; not spawn-time random.
6. `pickAxisDirection` np0 jitter 0: only left/up.

**Verify:** script exit 0; `npx tsc --noEmit`.

**Guards:** Do not add Vitest/Jest/Playwright. Do not import `@/` in the Node check (use relative `./ripple.ts`).

---

## Phase 2 — Plugin engine: fade retract + hard fill + spatial rainbow (circle)

**What:** Copy `waveRadius` and hard fill from `src/lib/ripple.ts:440-457` and `sampleRipples` fill branch into `RippleEngine.h` `SampleUnlocked`. Replace `pow(..., fade_power)` dimming.

**Copy from studio:**

- Retract: after `expand` (use `lifetime` as expand), radius = `peak * (1 - u)` over `fade` seconds.
- Rename or add `float fade = 1.0f` on `RippleSettings`. **Do not silently change `fade_power` meaning** without updating `RippleWidget.cpp:373` (`fade_->value() / 100.0f` currently → `fade_power`) and `RippleSettingsIO.h` keys. Prefer new field `fade` (seconds) and persist `"fade"`; keep reading `"fade_power"` as legacy or drop it in the same PR with a comment in `RippleSettingsIO.h`.
- Fill: `dist <= radius ? 1 : 0` (copy studio, not old `1 - dist/radius`).
- Rainbow: already `RainbowAtDistance(dist)` in the header — keep Euclidean `dist` for circle. `RainbowKeysPerCycle = 8` already matches TS.

**Widget:** Lifetime slider min is 35 (`0.35s`). Studio lifetime min is **0.05**. Copy studio range: change `add_slider("Lifetime", ..., 35, 280, 115)` in `RippleWidget.cpp:238` to min **5** (`0.05s`) to match `controls.tsx` lifetime `min={0.05}`. Fade slider `0-300` already allows 0; after remap, `0` = snap off (copy `waveRadius` `fade <= 1e-4`).

**Verify (hostless, copy Expect blocks):**

- Fade 0: after `lifetime`, `Sample` returns idle (not a dim ghost).
- Fade 3, lifetime 1, speed 10: at t=2.5 radius conceptually 5 — key at dist 9 idle, dist 2 on.
- Fill at dist 0.99*radius is **full** wave colour (not 1% wash).

**Guards:** Do not keep multiplying RGB by `pow(1-t, fade_power)` *and* retract (the “strangeness” we removed). Do not change blend math (`BlendLayer` already matches `blendLayer` in TS).

---

## Phase 3 — Plugin: Square + Row/Col + Sweep

**What:** Copy shape enum and `waveDist` / `pickAxisDirection` from `ripple.ts`.

**Copy:**

- `WaveShape` / `SHAPE_OPTIONS` labels Circle, Square, Row/Col, Sweep (`ripple.ts:4-35`). Dart is Phase 4.
- Chebyshev branch `ripple.ts:478-502`.
- `pickAxisDirection` `ripple.ts:157-195` (coin-flip axis; jitter = short-way chance).
- Sweep `spanLat = 0.45 + sweepSpan * full` (`spawnRipples` sweep block).
- `waveRadius(..., travel)` so lifetime is the crossing clock.

**Widget:** Add `QComboBox` shape **using the same widget types as Brush** (`RippleWidget.cpp` brush combo ~Ring/Fill/Soft). Add jitter + span sliders only when shape is axis/sweep (copy visibility from `controls.tsx:206-260`). Persist `shape`, `axis_jitter`, `sweep_span` in `RippleSettingsIO.h` the same way as `brush` (`JsonGet` / `SettingsToJson`).

**Verify:**

- Hostless: Chebyshev corner vs Euclidean (copy Phase 1 cases in C++).
- Hostless: jitter 0, spawn from bottom-right, many seeds → only left or up travel.
- `grep` plugin for `RippleShape` / `axis_jitter` / `sweep_span`.
- Linux: **cannot** click the Ripple tab. Proof = hostless + code review. User verifies on Windows.

**Guards:** Do not add Dart in this phase. Do not use Manhattan distance (that is a diamond, not square). Do not floor expand at `travel/speed` (that made lifetime < 1s look like ~1s).

---

## Phase 4 — Plugin: Dart + last-key blast

**What:** Copy Dart from studio. Design spec: `docs/superpowers/specs/2026-08-21-jump-modes-design.md` **First attempt: B — Dart**, but **code wins** where they disagree (Euclidean line, idle-from-keydown blast, wipe, arrival colour).

**Copy:**

- `spawnRipples` jump block + `from` (`ripple.ts:283-292`).
- `jumpCoverage` including `len < 0.2` (`ripple.ts:504-535`). **Export or duplicate** — it is currently file-private.
- Stage fuse: `keyboard-stage.tsx:186-208` (pending blast, `now - lastPressAt >= lifetime`, `filter` darts, `spawnBlast(..., lastDart)`).
- Wipe: on blast, drop non-blast ripples; while a live `blast`, skip dart sample (`sampleRipples` `blasting`).
- `dartArrivalColor` for Random/Rainbow blast (`ripple.ts`).
- UI: Trail, Explosion shape, Blast size from `controls.tsx` jump-only rows. Lifetime/Fade copy for jump.

**Plugin wiring notes (existing, do not invent):** last key comes from `KeyboardHook` + `KeyMap` (already used in `RippleWidget.cpp` `ConsumeKeys`). Idle timer: copy the 16 ms `QTimer` already used for `Paint()` (`RippleWidget.h` `timer_`) — on each tick, if jump and idle ≥ lifetime, spawn blast. Do not add a second timer class.

**Verify:**

- Hostless: same-key dart, point 10u away idle.
- Hostless: two-point dart at mid-flight: head on, ahead off; `trailLength=0` behind off.
- Hostless: `dartArrivalColor` random uses `dart.color`, not a new seed.
- Hostless: after blast `life` elapsed, sample at landing is idle.
- Widget: shape Dart shows trail/blast; other shapes hide them (copy `controls.tsx` `settings.shape === "jump"`).

**Guards:** Do not explode per dart (`jumpCoverage` must not have `boomAt`). Do not start blast from dart-land time; studio uses **last keydown**. Do not pick a new random colour in `spawnBlast` when `fromDart` is set. Do not implement Bolt path-on. Auto-demo 520 ms will never idle if lifetime is 1.15s — plugin has no auto-demo; ignore.

---

## Phase 5 — SDK client + READMEs

**What:** Rebuild client against live `../RippleEngine.h` (`OpenRGBRipplePlugin/sdk-client/CMakeLists.txt` already lists `../RippleEngine.h`). Update `RippleConfig.inc` blend/help: today `--blend over|add` only (`sdk-client/RippleConfig.inc` ~line 33). Copy blend names from `RippleWidget.cpp` blend combo.

**Copy:** Plugin README config table from `OpenRGBRipplePlugin/README.md` (stale Over/Add). After Phase 2–4, document Fade as seconds, Lifetime min 0.05, Shape, Dart.

**Do not** refresh `public/sdk-client/RippleEngine.h` as a second engine; either delete the snapshot or generate it from the plugin header in the existing site build. Canonical include is `__has_include("../RippleEngine.h")` in `OpenRGBRippleClient.cpp`.

**Verify:** `grep` `RippleConfig.inc` for Screen/XOR; `diff` plugin `RippleEngine.h` vs any shipped copy.

**Guards:** Do not run SDK client and plugin together (README already says so).

---

## Phase 6 — Later revisions (not this stack)

Only after Dart feels right on hardware. Copy from `docs/superpowers/specs/2026-08-21-jump-modes-design.md`:

- **A Bolt:** path keys on, then same runner (`jumpCoverage` + light `along <= head` the full path).
- **C1 Trail-then-snap:** trail = full path; fade retracts the comet (already have Fade).
- **C2 Ping-pong:** reverse `dir` until lifetime; explode at head. Lifetime meaning **changes** — decide in that session.

**Out of scope (spec):** geometric rubber-band, plugin-unrelated studio chrome, Bresenham unless you explicitly replace Euclidean projection.

---

## Phase 7 — Verification

1. `node --experimental-strip-types src/lib/ripple.check.ts` (Phase 1) — 0 fails.
2. `npx tsc --noEmit`.
3. `npm run build` (vite). Studio **8085** if demoing (`AGENTS.md`); user inspects — no Playwright.
4. Hostless: build `OpenRGBRipplePlugin/tests` target `check`.
5. Grep guards:
   - No `pow(.*fade_power` in `SampleUnlocked` after Phase 2.
   - No `1.0f - dist /` fill wash after Phase 2.
   - `shape` / `trailLength` or C++ equivalents present after Phase 4.
   - `len < 0.2` (or C++ equivalent) still present.
6. SDK: client compiles against `../RippleEngine.h`, not the stale public snapshot.
7. Windows (user): Ripple tab — Square fill crisp; Sweep lifetime 0.35s with fade 0 is 0.35s; Dart idle boom once; Random boom matches last dart; same key twice does not fill the board.

**Done when:** studio checks + hostless checks are green, and the plugin settings/JSON/widget expose Square, Row/Col, Sweep, Dart, retract Fade, and last-key blast without a second undocumented fade path.
