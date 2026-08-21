import {
  DEFAULT_BOUNDS,
  DEFAULT_SETTINGS,
  axisHeadingName,
  dartArrivalColor,
  pickAxisDirection,
  rainbowAtDistance,
  sampleRipples,
  spawnRipples,
  waveRadius,
  type RGB,
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
  const retract = waveRadius(10, 2.5, 1, 3);
  expect(retract != null && almost(retract, 5), "waveRadius(10, 2.5, 1, 3) → 5");
  expect(waveRadius(10, 1.01, 1, 0) === null, "waveRadius(10, 1.01, 1, 0) → null");
  const directed = waveRadius(14, 0.35, 0.35, 0, 14);
  expect(
    directed != null && almost(directed, 14),
    "waveRadius(14, 0.35, 0.35, 0, 14) → 14",
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
    origin.r > 200 && origin.r > origin.g && origin.r > origin.b,
    "rainbowAtDistance(0) red-ish",
  );
  const cyan = rainbowAtDistance(4);
  expect(
    cyan.g > 200 && cyan.b > 200 && cyan.r < 80,
    "rainbowAtDistance(4) cyan-ish",
  );
  const settings: RippleSettings = {
    ...fillSettings("circle"),
    colorMode: "rainbow",
  };
  const ripples = spawnRipples(settings, 0, 0, 0, 1, DEFAULT_BOUNDS);
  const at0 = sampleRipples(settings, ripples, 0, 0, 0.5);
  const at4 = sampleRipples(settings, ripples, 4, 0, 0.5);
  expect(at0.r > 200 && at0.r > at0.g, "sample rainbow at origin is red-ish");
  expect(at4.g > 200 && at4.b > 200 && at4.r < 80, "sample rainbow at dist 4 is cyan-ish");
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

if (fails) {
  console.error(fails + " failed");
  process.exit(1);
}
console.log("hostless tests ok");
process.exit(0);
