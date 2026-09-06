import {
  AXIS_VERTICAL_SPEED,
  DEFAULT_BOUNDS,
  DEFAULT_SETTINGS,
  axisHeadingName,
  dartArrivalColor,
  directedSpeed,
  pickAxisDirection,
  rainbowAtDistance,
  sampleRipples,
  spawnBlast,
  spawnRipples,
  waveRadius,
  type RGB,
  type Ripple,
  type RippleSettings,
} from "./ripple.ts";

let fails = 0;

function expect(ok: boolean, msg: string) {
  if (!ok) {
    console.error("FAIL: " + msg);
    fails++;
  }
}

function almost(a: number, b: number, eps = 1e-9): boolean {
  return Math.abs(a - b) <= eps;
}

function isIdle(c: RGB, idle: RGB): boolean {
  return (
    Math.abs(c.r - idle.r) <= 1 &&
    Math.abs(c.g - idle.g) <= 1 &&
    Math.abs(c.b - idle.b) <= 1
  );
}

function isLit(c: RGB): boolean {
  return c.r > 200 || c.g > 200 || c.b > 200;
}

function fillSettings(shape: RippleSettings["shape"]): RippleSettings {
  return {
    ...DEFAULT_SETTINGS,
    brush: "fill",
    shape,
    fade: 0,
    echoCount: 0,
    impactFlash: false,
    colorMode: "solid",
    solid: { r: 255, g: 0, b: 0 },
    idle: { r: 0, g: 0, b: 0 },
    brightness: 1,
    lifetime: 1,
    speed: 10,
  };
}

{
  const settings = fillSettings("square");
  const ripples = spawnRipples(settings, 0, 0, 0, 1, DEFAULT_BOUNDS);
  /* t=0.5, radius = speed * elapsed = 5. Fill is hard dist<=radius, not wash. */
  const inside = sampleRipples(settings, ripples, 4, 4, 0.5);
  expect(isLit(inside), "square fill Chebyshev inside is ON");
  const outside = sampleRipples(settings, ripples, 5.1, 0, 0.5);
  expect(isIdle(outside, settings.idle), "square fill 0.1 outside idle");
  const corner = sampleRipples(settings, ripples, 5, 5, 0.5);
  expect(isLit(corner), "square fill Chebyshev corner ON");
}

{
  const settings = fillSettings("circle");
  const ripples = spawnRipples(settings, 0, 0, 0, 1, DEFAULT_BOUNDS);
  /* Near-edge wash would be ~1% coverage; hard fill stays full wave colour. */
  const near = sampleRipples(settings, ripples, 4.95, 0, 0.5);
  expect(isLit(near), "circle fill near edge is hard, not wash");
  const out = sampleRipples(settings, ripples, 5.1, 0, 0.5);
  expect(isIdle(out, settings.idle), "circle fill 0.1 outside idle");
  const euclidCorner = sampleRipples(settings, ripples, 5, 5, 0.5);
  expect(isIdle(euclidCorner, settings.idle), "circle fill Euclidean corner idle");
}

{
  /* Ring occupancy: in-band is 1, else 0. Thickness still sets band width. */
  const settings: RippleSettings = {
    ...DEFAULT_SETTINGS,
    brush: "ring",
    shape: "circle",
    fade: 0,
    speed: 10,
    lifetime: 1,
    thickness: 1.15,
    blend: "max",
    impactFlash: false,
    echoCount: 0,
    colorMode: "solid",
    solid: { r: 255, g: 0, b: 0 },
    idle: { r: 0, g: 255, b: 255 },
    brightness: 1,
  };
  const ripples = spawnRipples(settings, 0, 0, 0, 1, DEFAULT_BOUNDS);
  const crest = sampleRipples(settings, ripples, 5, 0, 0.5);
  expect(
    crest.r >= 250 && crest.g <= 5 && crest.b <= 5,
    "ring occupancy crest is full red",
  );
  const inband = sampleRipples(settings, ripples, 5.8, 0, 0.5);
  expect(
    inband.r >= 250 && inband.g <= 5 && inband.b <= 5,
    "ring occupancy in-band is full red, not grey mix",
  );
  const out = sampleRipples(settings, ripples, 6.3, 0, 0.5);
  expect(
    out.r <= 5 && out.g >= 250 && out.b >= 250,
    "ring occupancy outside band is idle cyan",
  );
}

{
  /* Complementary Ring blends after occupancy snap. Over/max is the
     occupancy block above. Sample through spawn+sample, not blendLayer(1). */
  const base: RippleSettings = {
    ...DEFAULT_SETTINGS,
    brush: "ring",
    shape: "circle",
    fade: 0,
    speed: 10,
    lifetime: 1,
    thickness: 1.15,
    impactFlash: false,
    echoCount: 0,
    colorMode: "solid",
    solid: { r: 255, g: 0, b: 0 },
    idle: { r: 0, g: 255, b: 255 },
    brightness: 1,
  };
  const cases: Array<{
    blend: RippleSettings["blend"];
    onOk: (c: RGB) => boolean;
    onMsg: string;
  }> = [
    {
      blend: "add",
      onOk: (c) => c.r >= 250 && c.g >= 250 && c.b >= 250,
      onMsg: "ring occupancy add on-band is white",
    },
    {
      blend: "burn",
      onOk: (c) => c.r <= 5 && c.g <= 5 && c.b <= 5,
      onMsg: "ring occupancy burn on-band is black",
    },
    {
      blend: "overlay",
      onOk: (c) => c.r >= 250 && c.g <= 5 && c.b <= 5,
      onMsg: "ring occupancy overlay on-band is red",
    },
  ];
  for (const c of cases) {
    const settings = { ...base, blend: c.blend };
    const ripples = spawnRipples(settings, 0, 0, 0, 1, DEFAULT_BOUNDS);
    const on = sampleRipples(settings, ripples, 5, 0, 0.5);
    expect(c.onOk(on), c.onMsg);
    const off = sampleRipples(settings, ripples, 6.3, 0, 0.5);
    expect(
      off.r <= 5 && off.g >= 250 && off.b >= 250,
      "ring occupancy " + c.blend + " off-band is idle cyan",
    );
  }
}

{
  const retract = waveRadius(10, 2.5, 1, 3);
  expect(retract != null && almost(retract, 5), "waveRadius(10, 2.5, 1, 3) → 5");
  expect(waveRadius(10, 1.01, 1, 0) === null, "waveRadius(10, 1.01, 1, 0) → null");
  /* Sweep / Row-Col: speed is travel rate; lifetime caps expand. */
  expect(
    almost(waveRadius(14, 0.35, 1.15, 0, 20) ?? -1, 4.9),
    "directed uses speed, not lifetime-stretched crossing",
  );
  expect(
    waveRadius(14, 0.5, 1.15, 0, 5) === null,
    "directed fade 0 snaps off after hitting travel (max, not guaranteed)",
  );
  expect(
    almost(waveRadius(14, 0.3, 1.15, 0, 5) ?? -1, 4.2),
    "directed still expanding at speed before travel",
  );
  expect(
    waveRadius(10, 0.5, 0.4, 0, 20) === null,
    "lifetime caps expand before travel is reached",
  );
  expect(
    almost(waveRadius(10, 0.3, 0.4, 0, 20) ?? -1, 3),
    "lifetime cap: still expanding at speed",
  );
  const fromEdge = waveRadius(10, 1.0, 3, 2, 5);
  expect(
    fromEdge != null && almost(fromEdge, 3.75),
    "retract starts at travel, not at speed*lifetime",
  );
}

{
  const settings = fillSettings("axis");
  const base: Ripple = {
    x: 10,
    y: 3,
    t0: 0,
    color: settings.solid,
    echo: 0,
    travel: 20,
    expand: 1,
    life: 1,
  };
  const v: Ripple = { ...base, axis: "v", dir: -1 };
  const h: Ripple = { ...base, axis: "h", dir: 1 };
  expect(almost(AXIS_VERTICAL_SPEED, 0.7), "vertical axis speed factor is 0.7");
  expect(
    almost(directedSpeed(settings, v), 7),
    "row/col vertical speed is 0.7x",
  );
  expect(
    almost(directedSpeed(settings, h), 10),
    "row/col horizontal speed is unchanged",
  );
  expect(
    almost(directedSpeed(fillSettings("circle"), v), 10),
    "circle ignores axis speed factor",
  );
  /* t=0.4, speed 10 → h radius 4, v radius 2.8 */
  expect(
    isLit(sampleRipples(settings, [h], 13, 3, 0.4)),
    "row/col horizontal dist 3 still at full speed",
  );
  expect(
    isLit(sampleRipples(settings, [v], 10, 1.2, 0.4)),
    "row/col vertical 0.7x still covers dist 1.8",
  );
  expect(
    isIdle(sampleRipples(settings, [v], 10, 0, 0.4), settings.idle),
    "row/col vertical dist 3 not yet reached at 0.7x",
  );
  const sweep = { ...settings, shape: "sweep" as const, sweepSpan: 1 };
  expect(
    almost(directedSpeed(sweep, v), 7),
    "sweep vertical speed is 0.7x",
  );
  expect(
    isIdle(sampleRipples(sweep, [v], 10, 0, 0.4), sweep.idle),
    "sweep vertical dist 3 not yet reached at 0.7x",
  );
}

{
  const settings: RippleSettings = {
    ...DEFAULT_SETTINGS,
    shape: "jump",
    echoCount: 0,
    impactFlash: false,
    colorMode: "solid",
    solid: { r: 255, g: 0, b: 0 },
    idle: { r: 0, g: 0, b: 0 },
    brightness: 1,
  };
  const r = spawnRipples(settings, 0, 0, 0, 1, DEFAULT_BOUNDS, { x: 0, y: 0 });
  const far = sampleRipples(settings, r, 10, 0, 0.01);
  expect(isIdle(far, settings.idle), "same-key dart does not light a point 10 units away");
  const land = sampleRipples(settings, r, 0, 0, 0.01);
  expect(isLit(land), "same-key dart lights landing key");
  const on = sampleRipples(settings, r, 0.55, 0, 0.01);
  expect(isLit(on), "same-key dart d<=0.55 is lit");
}

{
  const origin = rainbowAtDistance(0);
  expect(
    origin.r >= 254 && origin.g <= 1 && origin.b <= 1,
    "rainbowAtDistance(0) true red",
  );
  const originBand = rainbowAtDistance(0.9);
  expect(
    originBand.r === origin.r &&
      originBand.g === origin.g &&
      originBand.b === origin.b,
    "rainbowAtDistance(0.9) equals rainbowAtDistance(0)",
  );
  const cyan = rainbowAtDistance(4);
  expect(
    cyan.r <= 1 && cyan.g >= 254 && cyan.b >= 254,
    "rainbowAtDistance(4) true cyan",
  );
  const cyanBand = rainbowAtDistance(4.9);
  expect(
    cyanBand.r === cyan.r && cyanBand.g === cyan.g && cyanBand.b === cyan.b,
    "rainbowAtDistance(4.9) equals rainbowAtDistance(4)",
  );
  const settings: RippleSettings = {
    ...fillSettings("circle"),
    colorMode: "rainbow",
  };
  const ripples = spawnRipples(settings, 0, 0, 0, 1, DEFAULT_BOUNDS);
  const at0 = sampleRipples(settings, ripples, 0, 0, 0.5);
  const at4 = sampleRipples(settings, ripples, 4, 0, 0.5);
  expect(
    at0.r >= 254 && at0.g <= 1 && at0.b <= 1,
    "sample rainbow at origin is true red",
  );
  expect(
    at4.r <= 1 && at4.g >= 254 && at4.b >= 254,
    "sample rainbow at dist 4 is true cyan",
  );
  expect(
    at0.r !== at4.r || at0.g !== at4.g,
    "rainbow is wave-distance, not spawn-time random",
  );
}

{
  expect(
    axisHeadingName({ axis: "h", dir: -1, travel: 1 }) === "left",
    "left is axis h dir -1",
  );
  expect(
    axisHeadingName({ axis: "v", dir: -1, travel: 1 }) === "up",
    "up is axis v dir -1",
  );
  for (let seed = 1; seed <= 64; seed++) {
    const h = pickAxisDirection(19.6, 5.5, DEFAULT_BOUNDS, seed, 0);
    const name = axisHeadingName(h);
    expect(name === "left" || name === "up", "np0 jitter 0 seed " + seed + " is left or up");
  }
}

{
  /* Integer LED coords (plugin matrix): far-edge row sits at dist==travel.
     fade 0 snaps that same instant, so 16ms ticks never paint F-row. */
  const bounds = { minX: 0, maxX: 21, minY: 0, maxY: 5 };
  const settings: RippleSettings = {
    ...fillSettings("sweep"),
    speed: 21,
    lifetime: 1.33,
    fade: 0,
    axisJitter: 0.02,
    sweepSpan: 1,
    thickness: 2.5,
  };
  let seed = 0;
  for (let s = 1; s <= 64; s++) {
    const h = pickAxisDirection(4, 4, bounds, s, 0.02);
    if (h.axis === "v" && h.dir < 0) {
      seed = s;
      break;
    }
  }
  expect(seed > 0, "found an up sweep seed from C");
  const ripples = spawnRipples(settings, 4, 4, 0, seed, bounds);
  let fRow = false;
  for (let t = 0; t <= 0.4; t += 0.016) {
    const c = sampleRipples(settings, ripples, 4, 0, t);
    if (c.r > 200) fRow = true;
  }
  expect(fRow, "sweep up from C lights F-row (y=0) on a 16ms tick, not only the number row");
}

{
  const settings: RippleSettings = {
    ...DEFAULT_SETTINGS,
    shape: "jump",
    colorMode: "random",
    echoCount: 0,
    impactFlash: false,
  };
  const [dart] = spawnRipples(settings, 4, 0, 0, 7, DEFAULT_BOUNDS, { x: 0, y: 0 });
  const arrival = dartArrivalColor(settings, dart);
  expect(
    arrival.r === dart.color.r &&
      arrival.g === dart.color.g &&
      arrival.b === dart.color.b,
    "dartArrivalColor random uses dart.color",
  );
}

{
  const settings: RippleSettings = {
    ...fillSettings("jump"),
    colorMode: "rainbow",
    blastSize: 12,
  };
  const dart = {
    x: 0,
    y: 0,
    t0: 0,
    color: rainbowAtDistance(4),
    echo: 0,
    travel: 4,
  };
  const blast = spawnBlast(settings, 0, 0, 0, 1, dart);
  const at0 = sampleRipples(settings, [blast], 0, 0, 0.5);
  const at4 = sampleRipples(settings, [blast], 4, 0, 0.5);
  expect(
    at0.r > 200 && at0.r > at0.g,
    "rainbow blast at origin is red-ish, not the dart's cyan arrival",
  );
  expect(
    at4.g > 200 && at4.b > 200 && at4.r < 80,
    "rainbow blast at dist 4 is cyan-ish",
  );
}

{
  /* Dart boom: expand is Lifetime, not blastSize/speed. fade 0 snaps at expand. */
  const settings: RippleSettings = {
    ...fillSettings("jump"),
    speed: 50,
    lifetime: 1,
    fade: 0,
    blastSize: 3.5,
  };
  const blast = spawnBlast(settings, 0, 0, 0, 1);
  expect(
    almost(blast.expand ?? 0, 1),
    "blast expand is lifetime, not size/speed (3.5/50 = 0.07)",
  );
  expect(
    isLit(sampleRipples(settings, [blast], 0, 0, 0.5)),
    "blast still expanding at t=0.5 when speed would have finished",
  );
  expect(
    isIdle(sampleRipples(settings, [blast], 0, 0, 1.01), settings.idle),
    "blast snaps at lifetime when fade 0",
  );
}

if (fails) {
  console.error(fails + " failed");
  process.exit(1);
}
console.log("hostless tests ok");
process.exit(0);
