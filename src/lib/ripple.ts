/** Shared Artemis-style key-press ripple engine. Keep in lockstep with RippleEngine.cpp */

export type BrushType = "ring" | "fill" | "soft";
export type WaveShape = "circle" | "square" | "axis" | "sweep" | "jump";
export type BlastShape = "circle" | "square";
export type ColorMode = "solid" | "rainbow" | "random";
export type Axis = "h" | "v";
export type BlendMode =
  | "max"
  | "add"
  | "xor"
  | "screen"
  | "overlay"
  | "dodge"
  | "burn"
  | "exclusion";

export const BLEND_OPTIONS: { id: BlendMode; label: string }[] = [
  { id: "max", label: "Over" },
  { id: "add", label: "Add" },
  { id: "xor", label: "XOR" },
  { id: "screen", label: "Screen" },
  { id: "overlay", label: "Overlay" },
  { id: "dodge", label: "Dodge" },
  { id: "burn", label: "Burn" },
  { id: "exclusion", label: "Exclusion" },
];

export const SHAPE_OPTIONS: { id: WaveShape; label: string }[] = [
  { id: "circle", label: "Circle" },
  { id: "square", label: "Square" },
  { id: "axis", label: "Row/Col" },
  { id: "sweep", label: "Sweep" },
  { id: "jump", label: "Dart" },
];

export const BLAST_SHAPE_OPTIONS: { id: BlastShape; label: string }[] = [
  { id: "circle", label: "Circle" },
  { id: "square", label: "Square" },
];

export interface LayoutBounds {
  minX: number;
  maxX: number;
  minY: number;
  maxY: number;
}

export const DEFAULT_BOUNDS: LayoutBounds = {
  minX: 0,
  maxX: 22.6,
  minY: 0,
  maxY: 6,
};

export interface RGB {
  r: number;
  g: number;
  b: number;
}

export interface RippleSettings {
  brush: BrushType;
  shape: WaveShape;
  speed: number;
  thickness: number;
  lifetime: number;
  /** Seconds to retract from the outer edge back to the key. 0 = snap off. */
  fade: number;
  echoCount: number;
  echoDelay: number;
  brightness: number;
  idle: RGB;
  colorMode: ColorMode;
  solid: RGB;
  impactFlash: boolean;
  impactHold: number;
  blend: BlendMode;
  /** Row/Col and Sweep: 0 = always the longer remaining path, 1 = 50/50. */
  axisJitter: number;
  /** Sweep: 0 = short bar around the key, 1 = full row or column. */
  sweepSpan: number;
  /** Dart: keys kept lit behind the head. 0 = blob only. */
  trailLength: number;
  /** Dart: explosion radius in key-widths, after idle lifetime. */
  blastSize: number;
  blastShape: BlastShape;
}

export const DEFAULT_SETTINGS: RippleSettings = {
  brush: "ring",
  shape: "circle",
  speed: 14,
  thickness: 1.15,
  lifetime: 1.15,
  fade: 1.0,
  echoCount: 1,
  echoDelay: 0.12,
  brightness: 1,
  idle: { r: 6, g: 8, b: 10 },
  colorMode: "rainbow",
  solid: { r: 46, g: 230, b: 214 },
  impactFlash: true,
  impactHold: 0.08,
  blend: "max",
  axisJitter: 0.18,
  sweepSpan: 1,
  trailLength: 2.5,
  blastSize: 3.5,
  blastShape: "circle",
};

export interface Ripple {
  x: number;
  y: number;
  t0: number;
  color: RGB;
  echo: number;
  /** Row/Col only: which lane and which way the pulse travels. */
  axis?: Axis;
  /** h: -1 left / +1 right. v: -1 up / +1 down. */
  dir?: number;
  /** Remaining keys in the chosen direction — used to stretch expand time. */
  travel?: number;
  /** Seconds spent travelling out (before fade retract). */
  expand?: number;
  life?: number;
  /** Sweep: how far the bar extends along the lit axis (key units). */
  spanLat?: number;
  /** Dart landing (takeoff is x,y). */
  tx?: number;
  ty?: number;
  /** One-shot explosion at the last key (not a dart). */
  blast?: boolean;
  blastShape?: BlastShape;
}

export interface AxisHeading {
  axis: Axis;
  dir: number;
  travel: number;
}

/** Deterministic 0..1 from a press seed. */
export function u01(seed: number, salt: number): number {
  let t = (Math.imul(seed, 0x9e3779b9) + salt) >>> 0;
  t = Math.imul(t ^ (t >>> 15), t | 1);
  t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
}

/**
 * Coin-flip axis (row vs column). On that axis, take the longer remaining
 * path (max time the wave stays on). `jitter` 0..1 is the chance of the
 * short way: 0 = always long, 1 = 50/50.
 */
export function pickAxisDirection(
  x: number,
  y: number,
  bounds: LayoutBounds,
  seed: number,
  jitter = 0,
): AxisHeading {
  const left = Math.max(0.08, x - bounds.minX);
  const right = Math.max(0.08, bounds.maxX - x);
  const up = Math.max(0.08, y - bounds.minY);
  const down = Math.max(0.08, bounds.maxY - y);

  const axis: Axis = u01(seed, 1) < 0.5 ? "h" : "v";
  const pShort = 0.5 * Math.max(0, Math.min(1, jitter));
  const takeShort = u01(seed, 2) < pShort;

  const pick = (
    a: number,
    b: number,
    dirA: number,
    dirB: number,
  ): { dir: number; travel: number } => {
    const aLong = a >= b;
    const long = aLong ? a : b;
    const short = aLong ? b : a;
    const longDir = aLong ? dirA : dirB;
    const shortDir = aLong ? dirB : dirA;
    return takeShort
      ? { dir: shortDir, travel: short }
      : { dir: longDir, travel: long };
  };

  if (axis === "h") {
    const { dir, travel } = pick(left, right, -1, 1);
    return { axis, dir, travel };
  }
  const { dir, travel } = pick(up, down, -1, 1);
  return { axis, dir, travel };
}

function headingLabel(h: AxisHeading): "left" | "right" | "up" | "down" {
  if (h.axis === "h") return h.dir < 0 ? "left" : "right";
  return h.dir < 0 ? "up" : "down";
}

export function axisHeadingName(h: AxisHeading): "left" | "right" | "up" | "down" {
  return headingLabel(h);
}

export function clampByte(n: number): number {
  return Math.max(0, Math.min(255, n | 0));
}

export function hsvToRgb(h: number, s: number, v: number): RGB {
  const i = Math.floor(h * 6);
  const f = h * 6 - i;
  const p = v * (1 - s);
  const q = v * (1 - f * s);
  const t = v * (1 - (1 - f) * s);
  switch (i % 6) {
    case 0:
      return { r: v * 255, g: t * 255, b: p * 255 };
    case 1:
      return { r: q * 255, g: v * 255, b: p * 255 };
    case 2:
      return { r: p * 255, g: v * 255, b: t * 255 };
    case 3:
      return { r: p * 255, g: q * 255, b: v * 255 };
    case 4:
      return { r: t * 255, g: p * 255, b: v * 255 };
    default:
      return { r: v * 255, g: p * 255, b: q * 255 };
  }
}

/** Key-widths per full hue cycle. Origin is red; outward walks the spectrum. */
export const RAINBOW_KEYS_PER_CYCLE = 8;

export function rainbowAtDistance(dist: number, hueOffset = 0): RGB {
  let h = (dist / RAINBOW_KEYS_PER_CYCLE + hueOffset) % 1;
  if (h < 0) h += 1;
  return hsvToRgb(h, 0.82, 1);
}

export function colorForPress(
  settings: RippleSettings,
  now: number,
  seed: number,
): RGB {
  if (settings.colorMode === "solid") return settings.solid;
  if (settings.colorMode === "rainbow") {
    /* Placeholder: sampleRipples replaces this with rainbowAtDistance. */
    return rainbowAtDistance(0);
  }
  const h = ((seed * 0.61803398875) % 1 + 1) % 1;
  return hsvToRgb(h, 0.78, 1);
}

export function spawnRipples(
  settings: RippleSettings,
  x: number,
  y: number,
  now: number,
  seed: number,
  bounds: LayoutBounds = DEFAULT_BOUNDS,
  from?: { x: number; y: number },
): Ripple[] {
  const color = colorForPress(settings, now, seed);
  let heading: AxisHeading | undefined;
  let expand = settings.lifetime;
  let spanLat: number | undefined;
  let ox = x;
  let oy = y;
  let tx: number | undefined;
  let ty: number | undefined;
  if (settings.shape === "axis" || settings.shape === "sweep") {
    heading = pickAxisDirection(x, y, bounds, seed, settings.axisJitter);
    if (settings.shape === "sweep") {
      const full =
        heading.axis === "h"
          ? bounds.maxY - bounds.minY
          : bounds.maxX - bounds.minX;
      const span = Math.max(0, Math.min(1, settings.sweepSpan));
      spanLat = 0.45 + span * full;
    }
  }
  if (settings.shape === "jump") {
    const takeoff = from ?? { x, y };
    ox = takeoff.x;
    oy = takeoff.y;
    tx = x;
    ty = y;
    const dist = Math.hypot(x - ox, y - oy);
    const flight = dist / Math.max(0.1, settings.speed);
    expand = flight + settings.lifetime;
  }
  const life = expand + Math.max(0, settings.fade);
  const make = (t0: number, echo: number): Ripple => ({
    x: ox,
    y: oy,
    t0,
    color,
    echo,
    axis: heading?.axis,
    dir: heading?.dir,
    travel: heading?.travel ?? (tx != null && ty != null
      ? Math.hypot(tx - ox, ty - oy)
      : undefined),
    expand,
    life,
    spanLat,
    tx,
    ty,
  });
  const out: Ripple[] = [make(now, 0)];
  for (let i = 1; i <= settings.echoCount; i++) {
    out.push(make(now + i * settings.echoDelay, i));
  }
  return out;
}

/** Colour the dart had at the landing — used for the explosion. */
export function dartArrivalColor(
  settings: RippleSettings,
  dart: Ripple,
): RGB {
  if (settings.colorMode === "rainbow") {
    return rainbowAtDistance(Math.max(0, dart.travel ?? 0));
  }
  return dart.color;
}

export function spawnBlast(
  settings: RippleSettings,
  x: number,
  y: number,
  now: number,
  seed: number,
  fromDart?: Ripple,
): Ripple {
  const size = Math.max(0.4, settings.blastSize);
  const expand = Math.max(0.08, size / Math.max(0.1, settings.speed));
  return {
    x,
    y,
    t0: now,
    color: fromDart
      ? dartArrivalColor(settings, fromDart)
      : colorForPress(settings, now, seed),
    echo: 0,
    travel: size,
    expand,
    life: expand + Math.max(0, settings.fade),
    blast: true,
    blastShape: settings.blastShape,
  };
}

function hardLight(s: number, d: number): number {
  return s < 0.5 ? 2 * s * d : 1 - 2 * (1 - s) * (1 - d);
}

function colorDodge(s: number, d: number): number {
  if (d <= 0) return s;
  if (s >= 1) return 1;
  return Math.min(1, d / (1 - s));
}

function colorBurn(s: number, d: number): number {
  if (s <= 0) return 0;
  return Math.max(0, 1 - (1 - d) / s);
}

/** Keep in lockstep with RippleEngine.h BlendLayer. */
export function blendLayer(
  src: RGB,
  dst: RGB,
  coverage: number,
  mode: BlendMode,
): RGB {
  if (coverage <= 0.002) return dst;
  if (coverage > 1) coverage = 1;
  const sr = src.r / 255;
  const sg = src.g / 255;
  const sb = src.b / 255;
  const dr = dst.r / 255;
  const dg = dst.g / 255;
  const db = dst.b / 255;
  let br = sr;
  let bg = sg;
  let bb = sb;
  switch (mode) {
    case "add":
      br = Math.min(1, sr + dr);
      bg = Math.min(1, sg + dg);
      bb = Math.min(1, sb + db);
      break;
    case "xor":
      br = Math.abs(sr - dr);
      bg = Math.abs(sg - dg);
      bb = Math.abs(sb - db);
      break;
    case "screen":
      br = 1 - (1 - sr) * (1 - dr);
      bg = 1 - (1 - sg) * (1 - dg);
      bb = 1 - (1 - sb) * (1 - db);
      break;
    case "overlay":
      br = hardLight(sr, dr);
      bg = hardLight(sg, dg);
      bb = hardLight(sb, db);
      break;
    case "dodge":
      br = colorDodge(sr, dr);
      bg = colorDodge(sg, dg);
      bb = colorDodge(sb, db);
      break;
    case "burn":
      br = colorBurn(sr, dr);
      bg = colorBurn(sg, dg);
      bb = colorBurn(sb, db);
      break;
    case "exclusion":
      br = sr + dr - 2 * sr * dr;
      bg = sg + dg - 2 * sg * dg;
      bb = sb + db - 2 * sb * db;
      break;
    default:
      break;
  }
  const keep = 1 - coverage;
  return {
    r: (br * coverage + dr * keep) * 255,
    g: (bg * coverage + dg * keep) * 255,
    b: (bb * coverage + db * keep) * 255,
  };
}

/**
 * Expand at `speed` for up to `expand` seconds, then retract over `fade`.
 * `maxRadius` (Sweep / Row-Col remaining travel, blast size) caps how far
 * the front goes: lifetime is a maximum, not a guaranteed crossing time.
 * Hitting the cap or the far edge starts fade immediately (no hold).
 */
export function waveRadius(
  speed: number,
  elapsed: number,
  expand: number,
  fade: number,
  maxRadius?: number,
): number | null {
  if (elapsed < 0) return null;
  const expandT = Math.max(expand, 1e-6);
  const speedT = Math.max(speed, 1e-6);
  let peak = speedT * expandT;
  if (maxRadius != null && maxRadius > 0) peak = Math.min(peak, maxRadius);
  const outward = peak / speedT;
  if (elapsed <= outward) return speedT * elapsed;
  if (fade <= 1e-4) return null;
  const u = (elapsed - outward) / fade;
  if (u >= 1) return null;
  return peak * (1 - u);
}

function brushCoverage(
  brush: BrushType,
  dist: number,
  radius: number,
  thickness: number,
): number {
  if (brush === "ring") {
    const t = Math.abs(dist - radius) / Math.max(0.05, thickness);
    return t >= 1 ? 0 : 1 - t * t;
  }
  if (brush === "fill") {
    if (dist <= radius) return 1 - dist / Math.max(radius, 0.001);
    return 0;
  }
  const sigma = Math.max(0.15, thickness * 0.85);
  const d = dist - radius;
  return Math.exp(-(d * d) / (2 * sigma * sigma));
}

function waveDist(
  settings: RippleSettings,
  ripple: Ripple,
  dx: number,
  dy: number,
): { dist: number; onWave: boolean } {
  if (
    (settings.shape === "axis" || settings.shape === "sweep") &&
    ripple.axis &&
    ripple.dir
  ) {
    const along =
      ripple.axis === "h" ? dx * ripple.dir : dy * ripple.dir;
    const lateral =
      ripple.axis === "h" ? Math.abs(dy) : Math.abs(dx);
    const laneHalf =
      settings.shape === "sweep" ? (ripple.spanLat ?? 99) : 0.52;
    if (lateral > laneHalf || along < -0.4) return { dist: 0, onWave: false };
    return { dist: Math.max(0, along), onWave: true };
  }
  if (settings.shape === "square") {
    return { dist: Math.max(Math.abs(dx), Math.abs(dy)), onWave: true };
  }
  return { dist: Math.hypot(dx, dy), onWave: true };
}

function jumpCoverage(
  settings: RippleSettings,
  ripple: Ripple,
  x: number,
  y: number,
  elapsed: number,
): { coverage: number; dist: number } | null {
  const tx = ripple.tx ?? ripple.x;
  const ty = ripple.ty ?? ripple.y;
  const len = ripple.travel ?? Math.hypot(tx - ripple.x, ty - ripple.y);
  /* Same key twice (or first press): no path. A zero-length segment
     projects every LED to along=0, lateral=0 and paints the whole board. */
  if (len < 0.2) {
    const d = Math.hypot(x - tx, y - ty);
    return { coverage: d <= 0.55 ? 1 : 0, dist: 0 };
  }
  const ux = (tx - ripple.x) / len;
  const uy = (ty - ripple.y) / len;
  const along = (x - ripple.x) * ux + (y - ripple.y) * uy;
  const lateral = Math.abs((x - ripple.x) * uy - (y - ripple.y) * ux);
  const flight = len / Math.max(0.1, settings.speed);
  const head = elapsed < flight ? settings.speed * elapsed : len;
  const half = Math.max(0.45, settings.thickness * 0.4);
  if (lateral > half) return { coverage: 0, dist: along };
  const trail = Math.max(0, settings.trailLength);
  if (trail <= 0.02) {
    const onHead = Math.abs(along - head) <= 0.55;
    return { coverage: onHead ? 1 : 0, dist: along };
  }
  const onTrail = along <= head + 0.35 && along >= head - trail;
  return { coverage: onTrail ? 1 : 0, dist: along };
}

export function sampleRipples(
  settings: RippleSettings,
  ripples: readonly Ripple[],
  x: number,
  y: number,
  now: number,
): RGB {
  let r = settings.idle.r;
  let g = settings.idle.g;
  let b = settings.idle.b;
  const blasting = ripples.some(
    (rp) => rp.blast && now - rp.t0 <= (rp.life ?? 0),
  );

  for (const ripple of ripples) {
    const elapsed = now - ripple.t0;
    if (elapsed < 0) continue;

    if (ripple.blast) {
      const expand = ripple.expand ?? 0.18;
      const radius = waveRadius(
        settings.speed,
        elapsed,
        expand,
        settings.fade,
        ripple.travel,
      );
      if (radius == null) continue;
      const dx = x - ripple.x;
      const dy = y - ripple.y;
      const dist =
        (ripple.blastShape ?? "circle") === "square"
          ? Math.max(Math.abs(dx), Math.abs(dy))
          : Math.hypot(dx, dy);
      const coverage =
        settings.brush === "fill"
          ? dist <= radius
            ? 1
            : 0
          : brushCoverage(settings.brush, dist, radius, settings.thickness);
      if (coverage <= 0.002) continue;
      const src = {
        r: ripple.color.r * settings.brightness,
        g: ripple.color.g * settings.brightness,
        b: ripple.color.b * settings.brightness,
      };
      const mixed = blendLayer(src, { r, g, b }, coverage, settings.blend);
      r = mixed.r;
      g = mixed.g;
      b = mixed.b;
      continue;
    }

    if (settings.shape === "jump") {
      if (blasting) continue;
      const hit = jumpCoverage(settings, ripple, x, y, elapsed);
      if (!hit || hit.coverage <= 0.002) continue;
      const wave =
        settings.colorMode === "rainbow"
          ? rainbowAtDistance(Math.max(0, hit.dist))
          : ripple.color;
      const src = {
        r: wave.r * settings.brightness,
        g: wave.g * settings.brightness,
        b: wave.b * settings.brightness,
      };
      const mixed = blendLayer(src, { r, g, b }, hit.coverage, settings.blend);
      r = mixed.r;
      g = mixed.g;
      b = mixed.b;
      continue;
    }

    const expand = ripple.expand ?? settings.lifetime;
    const radius = waveRadius(
      settings.speed,
      elapsed,
      expand,
      settings.fade,
      ripple.travel,
    );
    if (radius == null) continue;

    const dx = x - ripple.x;
    const dy = y - ripple.y;
    const hypot = Math.hypot(dx, dy);
    const { dist, onWave } = waveDist(settings, ripple, dx, dy);

    let coverage = 0;
    if (onWave) {
      // Fill is a hard front. Fade is a retract of that front, not a wash.
      coverage =
        settings.brush === "fill"
          ? dist <= radius
            ? 1
            : 0
          : brushCoverage(settings.brush, dist, radius, settings.thickness);
    }

    if (settings.impactFlash && hypot < 0.55) {
      const hold = settings.impactHold;
      const flash =
        elapsed < hold
          ? 1
          : Math.max(0, 1 - (elapsed - hold) / (expand * 0.35));
      coverage = Math.max(coverage, flash);
    }

    if (coverage <= 0.002) continue;

    const wave =
      settings.colorMode === "rainbow"
        ? rainbowAtDistance(dist)
        : ripple.color;
    const src = {
      r: wave.r * settings.brightness,
      g: wave.g * settings.brightness,
      b: wave.b * settings.brightness,
    };
    const out = blendLayer(src, { r, g, b }, coverage, settings.blend);
    r = out.r;
    g = out.g;
    b = out.b;
  }

  return { r: clampByte(r), g: clampByte(g), b: clampByte(b) };
}

export function pruneRipples(ripples: Ripple[], now: number, lifetime: number) {
  let w = 0;
  for (let i = 0; i < ripples.length; i++) {
    const ripple = ripples[i];
    const life = ripple.life ?? lifetime;
    if (now - ripple.t0 <= life) {
      ripples[w++] = ripple;
    }
  }
  ripples.length = w;
}

export function rgbCss(c: RGB, a = 1): string {
  return `rgba(${clampByte(c.r)},${clampByte(c.g)},${clampByte(c.b)},${a})`;
}
