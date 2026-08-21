# Jump modes (last key → this key)

Studio-only for now. Grid is **~22 × 6 keys** (ANSI layout in `keyboard-layout.ts`). There is no pixel line — only keys whose centers sit near a path. Adjacent keys have **no in-between**. Diagonals are stair-steps. A rubber-band *shape* does not read.

Shared for every jump mode:

- **First press:** no previous key. Arm origin only (small spark / no jump).
- **Later presses:** takeoff = last press, landing = this press.
- **Speed:** keys per second along the path (Bresenham / DDA through key centres).
- **Lifetime:** not flight time. Fuse after the jump’s “arrival” (hang, then explode). Flight time is always `distance / speed` so close ≠ far.
- **Explosion:** at end of lifetime, at the **landing** key. Reuse an existing burst (Square or Circle Fill, short expand, fade 0 or a small retract). Optional later: flash the path keys.
- **Do not run** at the same time as a competing painter on that press (one shape per press, same as today).

Path = keys nearest the straight line from takeoff centre to landing centre. Empty path (neighbours) → just the two keys.

---

## First attempt: B — Dart

**Ship this first.** One moving occupancy. That is what 22×6 can show.

- A blob walks last → current. Time to land = path length / Speed.
- **Trail** slider (key-widths behind the head): `0` = blob only; `2–4` = short comet; high = almost the whole path (C1 preview).
- On arrival, sit for **Lifetime**, then **explode** on the landing key (small hard fill, Fade retracts it).
- First press: no previous key, so flight is 0 — fuse then boom on that key, and it becomes the origin.

Why first: Speed × distance is the whole point; no line-drawing; explosion is a second beat we already know how to paint; ping-pong and bolt are flags on the same traveler later.

---

## Later revision: A — Bolt

Instant **path on** (all keys on the line light), then a **runner** (same dart) along that line.

- Neighbour pairs look like a double-press — weak. Far pairs read as a drawn jump.
- Speed = runner. Lifetime = hang after the runner arrives, then explosion at landing.
- Toggle vs Dart: “draw the track” on/off. Same physics.

Do not build until Dart feels right.

---

## Later revision: C — not geometric stretch

A taut rubber band does not exist at this resolution. Growing a band is “keys popping on in order” = Dart + trail.

Keep **C** as two later variants:

### C1 — Trail-then-snap (stretch stand-in)

Dart leaves **all** path keys lit behind it (comet). After lifetime, the trail **snaps**: explosion at landing and/or a flash that clears the trail from the far end back (reuse Fade-as-retract on the path).

### C2 — Ping-pong

Same traveler **reflects** landing ↔ takeoff until Lifetime runs out, then explodes at wherever the head is (or always at landing). Close keys strobe; far keys volley. Lifetime = rally length, which **does** fight the “lifetime = fuse” rule — decide when building C2 (rally time vs bounce count).

---

## Slider mapping (Dart / Bolt)

| Control | Jump |
|---|---|
| Speed | Flight along the path |
| Lifetime | Fuse after arrival, then boom |
| Fade | Explosion retract only (0 = snap boom) |
| Echoes | Extra darts delayed on the same path (later) |

Do not use Lifetime as flight time (same lie Sweep had: one duration for every distance).

---

## Out of scope until revisions

- Plugin / SDK port
- Geometric rubber-band, curves, thickness-as-pixels
- Multi-jump history (snake of all keys this session)
- Explosion as a new brush; first pass = existing Square or Circle Fill at landing
