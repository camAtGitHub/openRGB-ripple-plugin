# Implementation plan: occupancy snap + saturated rainbow + version 1.0.10

Execute **one phase per session**. Copy from the cited files; do not invent settings, Qt widgets, palettes, or test runners. Product target is the **OpenRGB plugin** (`RippleEngine.h`). Keep `src/lib/ripple.ts` in lockstep so hostless TS checks match hardware. Studio on **8085** (`AGENTS.md`). **No Playwright** — user inspects the keyboard (and the page if demoing).

Agreed in conversation (plugin, Ring, cyan idle vs solid red):

- White / rose ring edges are **coverage lerp** of complementary colours, not Overlay magic.
- Occupancy snap: a Ring LED is **fully on or fully off**. Blend modes then run on full colours.
- Rainbow: **sat = 1** and **hue hops per whole key-width**. Not an 8-entry `pal()`. Eight is the existing spatial period (`RAINBOW_KEYS_PER_CYCLE`), not a hardware cap.
- **Do not change `BlendLayer` recipes.** Over / Add / XOR / Screen / Overlay / Dodge / Burn / Exclusion stay. Occupancy feeds them; it does not replace them.
- Do **not** remap the board onto a hardcoded `vec2(22, 6)`. `DeviceSession` already samples integer matrix `(x, y)`.

---

## Phase 0 — Allowed APIs (discovery)

### Engines (copy these; they already exist)

| API | Location |
|---|---|
| `rainbowAtDistance(dist, hueOffset?)` → `hsvToRgb(h, 0.82, 1)` | `src/lib/ripple.ts:234-241` |
| `RAINBOW_KEYS_PER_CYCLE = 8` | `src/lib/ripple.ts:235` |
| `hsvToRgb(h, s, v)` | `src/lib/ripple.ts:212-232` |
| Live Ring coverage `1 - t²` | `src/lib/ripple.ts:481-489` via `sampleRipples` `651-659` |
| Live Fill coverage `dist <= radius ? 1 : 0` | `src/lib/ripple.ts:654-658` (and blast `593-598`) |
| Soft Gaussian | `src/lib/ripple.ts:495-497` |
| `blendLayer(src, dst, coverage, mode)` + `keep = 1 - coverage` lerp | `src/lib/ripple.ts:372-435` |
| `RainbowAtDistance` → `HsvToRgb(h, 0.82f, 1.0f)` | `OpenRGBRipplePlugin/RippleEngine.h:444-455` |
| `RainbowKeysPerCycle = 8.0f` | `RippleEngine.h:445` |
| Ring `1-t²` / Fill hard / Soft Gaussian inlined | `RippleEngine.h:811-832` (blast `731-747`) |
| `BlendLayer` + keep lerp | `RippleEngine.h:478-557` |
| `SampleOver(x, y, now, base)` | `RippleEngine.h:374-377` |
| `Paint()` one sample per mapped LED | `OpenRGBRipplePlugin/RippleWidget.cpp:813-828` |
| Matrix `led.x = (float)x; led.y = (float)y` | `OpenRGBRipplePlugin/DeviceSession.cpp:76-90` |

**Do not copy as live Fill:** `brushCoverage` fill branch `ripple.ts:491-494` is a wash (`1 - dist/radius`). Live Fill is the hard front in `sampleRipples`.

### Tests / runners (exist)

| Runner | Location |
|---|---|
| `npm run test:ripple` | `package.json:9` → `node --experimental-strip-types src/lib/ripple.check.ts` |
| TS Expect helper | `src/lib/ripple.check.ts:19-26` |
| TS rainbow (red-ish / cyan-ish) | `ripple.check.ts:188-211` |
| C++ `Expect` | `OpenRGBRipplePlugin/tests/hostless/test_ripple_engine.cpp:10-17` |
| C++ Ring 1 unit off crest | `test_ripple_engine.cpp:82-107` |
| C++ `BlendLayer` at coverage `1.0f` | `test_ripple_engine.cpp:109-143` |
| CMake `check` | `OpenRGBRipplePlugin/tests/CMakeLists.txt:5-13` |

### Version (current **1.0.9** — bump together in Phase 4)

| Field | Location |
|---|---|
| `VERSION = 1.0.9;` | `OpenRGBRipplePlugin/OpenRGBRipplePlugin.pro:7` |
| `VERSION_STRING="1.0.9"` | `OpenRGBRipplePlugin/CMakeLists.txt:47` |
| `info.Version = "1.0.9"` | `OpenRGBRipplePlugin/OpenRGBRipplePlugin.cpp:23` |
| `QLabel("Ripple 1.0.9")` | `OpenRGBRipplePlugin/RippleWidget.cpp:141` |

`package.json` has no `"version"`. SDK client has no app semver. `OpenRGBRipplePlugin.json` has no version field. `VERSION_STRING` is defined and unused — do not invent a refactor to use it unless you are already touching those four literals.

### Locked behavior (this stack)

- **Ring occupancy:** after Ring coverage is computed, `coverage > 0.002` → `1`, else `0`. Thickness still sets band width (`t = \|dist-radius\| / max(0.05, thickness)`; `t >= 1` stays 0). A key in the band is the **full** wave colour.
- **Fill / Dart / Soft:** unchanged. Fill and Dart are already 0/1. Soft stays Gaussian.
- **`BlendLayer`:** do not edit the mode recipes or the keep lerp. With Ring coverage 0 or 1 the lerp becomes a no-op, which is the point.
- **Rainbow:** `sat = 1`. Hue = `floor(max(0, dist)) / RAINBOW_KEYS_PER_CYCLE` (then `% 1`). Origin is `(255, 0, 0)`, not `(255, 46, 46)`. Keep `RAINBOW_KEYS_PER_CYCLE = 8` as the wheel period. Do **not** add an 8-colour lookup table.
- **Random press colour:** leave sat `0.78` (out of scope). Solid unchanged.
- **No new settings, JSON keys, or widgets.**

### Complementary-colour outcomes (Ring, solid red, cyan idle) — after snap

Measured today (partial coverage): Over ≈ red with white/rose edges; Add/Screen/XOR/Exclusion/Dodge ≈ soft white; Color Burn ≈ off.

After occupancy snap, **on-band** LED (coverage 1):

| Blend | Result |
|---|---|
| Over (`Max`) | `(255, 0, 0)` |
| Overlay | `(255, 0, 0)` for this pair (Hard Light of primaries) |
| Add / Screen / XOR / Exclusion / Dodge | `(255, 255, 255)` |
| Color Burn | `(0, 0, 0)` |

Off-band stays idle cyan. White **edges** go away; Add of red+cyan is still white (RGB). That is required, not a bug.

### Anti-patterns (Phase 0)

- Do **not** hardcode `22×6` / `vec2 KEYS` / shader `cell()`. Use existing matrix / key centres.
- Do **not** invent `RippleEngine.cpp` (header-only). `ripple.ts:1` saying `.cpp` is stale.
- Do **not** change `BlendLayer` to winner-takes-all / channel `max` across modes.
- Do **not** replace HSV with a `pal()` of 8 named colours.
- Do **not** occupancy-snap Soft (Gaussian tail at 0.002 would light most of the board).
- Do **not** occupancy-snap Fill (already hard) or Dart (`jumpCoverage` already 0/1).
- Do **not** use Playwright. Do **not** load the Windows DLL on Linux as “tests passed.”
- Do **not** bump `OPENRGB_PLUGIN_API_VERSION` (stays 4).
- Do **not** treat `docs/.../2026-08-21-studio-modes-implementation-plan.md` Phase 0 as live (`VERSION = 1.0.6` and `fade_power` are stale).
- Do **not** edit `public/sdk-client/RippleEngine.h` if it still exists as a snapshot; canonical include is `OpenRGBRipplePlugin/sdk-client/` → `../RippleEngine.h`.

---

## Phase 1 — Ring occupancy snap (both engines)

**What:** After Ring coverage is computed in the live sample path, snap to 0 or 1. Copy the existing Ring `t` test; only the value on the in-band side changes (`1 - t²` → `1`).

**Copy from:**

- Studio Ring `t` + skip: `src/lib/ripple.ts:481-489` and apply at `sampleRipples` `651-659` (blast Ring is the `brushCoverage` branch `593-598` — snap there too when brush is ring).
- Plugin Ring inlined: `RippleEngine.h:814-820` (main) and `736-740` (blast). Same snap after `coverage` is set for `RippleBrush::Ring`.
- C++ Ring test already expects “mostly red” 1 unit off crest (`test_ripple_engine.cpp:82-107`) — it must still pass and should now be **fully** red.

**Put tests in:** `src/lib/ripple.check.ts` (Expect helper `:19-26`) and `test_ripple_engine.cpp` (Expect `:10-17`). Do **not** add Vitest/Jest.

**Cases:**

1. Solid red, idle cyan `(0,255,255)`, Ring, fade 0, speed 10, lifetime 1, thickness 1.15, Over, impact off, echo 0. Spawn `(0,0)` at t=0. At t=0.5 radius=5:
   - Sample `(5, 0)`: `r >= 250`, `g <= 5`, `b <= 5` (full red, not grey).
   - Sample `(5.8, 0)`: still in band (`t = 0.8/1.15 < 1`) → **full red**, not `red*0.5+cyan*0.5` grey. This is the white-edge bug.
   - Sample `(6.3, 0)`: `t >= 1` → idle cyan `r <= 5`, `g >= 250`, `b >= 250`.
2. Fill at the same times/points still hard-front (existing circle/square fill tests stay green).
3. Existing C++ “ring 1 unit off crest” (`test_ripple_engine.cpp:82-107`): tighten to `r > 250 && b < 10` if it still matches thickness 2.6 at t=2 (radius 2, sample x=1 → `t = 1/2.6 < 1`).

**Verify:**

```bash
npm run test:ripple
make -C OpenRGBRipplePlugin/tests/build test_ripple_engine && OpenRGBRipplePlugin/tests/build/test_ripple_engine
```

(If `tests/build` is missing: `cmake -S OpenRGBRipplePlugin/tests -B OpenRGBRipplePlugin/tests/build && cmake --build OpenRGBRipplePlugin/tests/build` — `cmake` may be absent; `make -C …/tests/build` is what this host used.)

**Guards:** Do not edit `blendLayer` / `BlendLayer`. Do not snap Soft. Do not change thickness units. Do not add a setting.

---

## Phase 2 — Saturated, quantized rainbow

**What:** In `rainbowAtDistance` / `RainbowAtDistance` only: saturation `1`, hue from `floor(dist)`. Keep `RAINBOW_KEYS_PER_CYCLE = 8`.

**Copy from:**

- Studio: `src/lib/ripple.ts:234-241`. Change the body to:

  ```ts
  const band = Math.floor(Math.max(0, dist));
  let h = (band / RAINBOW_KEYS_PER_CYCLE + hueOffset) % 1;
  if (h < 0) h += 1;
  return hsvToRgb(h, 1, 1);
  ```

- Plugin: `RippleEngine.h:444-455` — same formula (`std::floor`, `HsvToRgb(h, 1.0f, 1.0f)`).
- Existing TS rainbow asserts: `ripple.check.ts:188-211`. Tighten origin to true red; dist 4 stays cyan (`floor(4)/8 = 0.5`).

**Cases:**

1. `rainbowAtDistance(0)` → `r >= 254 && g <= 1 && b <= 1` (not sat-0.82 pink `(255, ~46, ~46)`).
2. `rainbowAtDistance(0.9)` equals `rainbowAtDistance(0)` (same band).
3. `rainbowAtDistance(4)` → `r <= 1 && g >= 254 && b >= 254`.
4. `rainbowAtDistance(4.9)` equals `rainbowAtDistance(4)`.
5. Sample path: fill circle, rainbow, t=0.5, speed 10 — origin still red, dist 4 still cyan (existing sample asserts, tightened).
6. C++: add the same four `RainbowAtDistance` numeric checks next to the existing rainbow-blast block (`test_ripple_engine.cpp:576-606`). There is **no** standalone C++ origin/cyan test today — copy the TS cases, do not invent a second palette.

**Verify:** same two hostless commands as Phase 1.

**Guards:** Do not add `pal()`. Do not change `RAINBOW_KEYS_PER_CYCLE`. Do not change Random sat `0.78`. Do not change `hsvToRgb` / `HsvToRgb` signatures. Blast / dart / circle already call `rainbowAtDistance(dist)` / `RainbowAtDistance(dist)` — they pick up the new function; do not fork a second rainbow.

---

## Phase 3 — Blend modes still distinct on a snapped Ring

**What:** Hostless cases that lock the complementary table in Phase 0. Sampling through the engine (not only `BlendLayer(..., 1.0f)`), so occupancy snap + blend both have to be right.

**Copy from:**

- Engine spawn+sample pattern: `test_ripple_engine.cpp:82-107` (Ring, solid, idle, Over).
- `BlendLayer` unit tests stay as-is at coverage `1.0f` (`test_ripple_engine.cpp:109-143`) — they already describe full-coverage mode math. Do **not** rewrite them.
- TS: copy the same Ring spawn from Phase 1; switch `blend`.

**Setup (both languages):** Ring, solid `{255,0,0}`, idle `{0,255,255}`, fade 0, speed 10, lifetime 1, thickness 1.15, impact off, echo 0, spawn `(0,0)`, sample `(5,0)` at t=0.5 (on-band).

| `blend` | on-band sample |
|---|---|
| `"max"` / `RippleBlend::Max` | red `(>=250, <=5, <=5)` |
| `"add"` / `Add` | white `(>=250, >=250, >=250)` |
| `"burn"` / `ColorBurn` | black `(<=5, <=5, <=5)` |
| `"overlay"` / `Overlay` | red, same as Over for this pair |

Off-band `(6.3, 0)` is idle cyan for all four.

**Verify:** `npm run test:ripple` and hostless C++ `test_ripple_engine`.

**Guards:** Do not add a new blend enum. Do not make Overlay differ from Over for this pair (Hard Light of pure red on cyan **is** red — `RippleEngine.h:520-527`). Do not weaken existing XOR orange/teal tests.

---

## Phase 4 — Version 1.0.9 → 1.0.10

**What:** Bump the four live literals together. No new version API.

**Copy from (replace `1.0.9` with `1.0.10`):**

```
OpenRGBRipplePlugin/OpenRGBRipplePlugin.pro:7
VERSION = 1.0.9;

OpenRGBRipplePlugin/CMakeLists.txt:47
    VERSION_STRING="1.0.9"

OpenRGBRipplePlugin/OpenRGBRipplePlugin.cpp:23
    info.Version     = "1.0.9";

OpenRGBRipplePlugin/RippleWidget.cpp:141
    auto* title = new QLabel("Ripple 1.0.9");
```

**Optional one-line README:** `OpenRGBRipplePlugin/README.md` Color row (`:43`) — keep `Rainbow / Solid / Random`. If you add a phrase, it is “Rainbow is saturated hue bands per key-width,” not a new control.

**Verify:**

```bash
rg -n "1\\.0\\.9" OpenRGBRipplePlugin --glob '!tests/build/**' --glob '!**/OpenRGB/**'
```

Expect **zero** product-version hits. Historical comments (e.g. `DetectionLifecycle.h` `1.0.4`) stay. `OPENRGB_PLUGIN_API_VERSION=4` stays.

**Guards:** Do not add `"version"` to `package.json`. Do not version the SDK client. Do not invent a `.rc` file. Do not wire `VERSION_STRING` into `GetPluginInfo` in this phase (optional hygiene, not required).

---

## Phase 5 — Verification

1. `npm run test:ripple` — 0 fails (Phase 1–3 cases present).
2. `npx tsc --noEmit`.
3. Hostless C++: `make -C OpenRGBRipplePlugin/tests/build test_ripple_engine && OpenRGBRipplePlugin/tests/build/test_ripple_engine` — 0 fails.
4. Grep guards:
   - `hsvToRgb(h, 1, 1)` / `HsvToRgb(h, 1.0f, 1.0f)` in the rainbow functions (no remaining `0.82` in those two functions).
   - Ring snap present: after Ring coverage, `coverage > 0.002` (or C++ equivalent) sets `1`.
   - `BlendLayer` / `blendLayer` keep lerp **still present** (`keep = 1 - coverage`).
   - No `pal(`, no `KEYS = vec2`, no `22.0, 6.0` shader remap.
   - Four version sites are `1.0.10`.
5. `npm run build` if touching studio. Demo on **8085** only if the user wants a preview — plugin is the product.
6. Windows (user, not this Linux host): Ripple tab **1.0.10**. Ring + Over + cyan background + solid red → red keys, **no white halo**. Add → hard white ring. Color Burn → hard black ring. Rainbow origin key is real red, not pink. Soft still has a falloff. Blend dropdown still has the same eight names.

**Done when:** both hostless suites are green, version is 1.0.10 in all four sites, Ring occupancy is 0/1, rainbow sat is 1 with `floor(dist)` bands, and `BlendLayer` recipes are unchanged.
