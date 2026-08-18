/** Shared Artemis-style key-press ripple engine. Keep in lockstep with RippleEngine.cpp */

export type BrushType = "ring" | "fill" | "soft";
export type ColorMode = "solid" | "rainbow" | "random";
export type BlendMode = "max" | "add";

export interface RGB {
  r: number;
  g: number;
  b: number;
}

export interface RippleSettings {
  brush: BrushType;
  speed: number;
  thickness: number;
  lifetime: number;
  fadePower: number;
  echoCount: number;
  echoDelay: number;
  brightness: number;
  idle: RGB;
  colorMode: ColorMode;
  solid: RGB;
  impactFlash: boolean;
  impactHold: number;
  blend: BlendMode;
}

export const DEFAULT_SETTINGS: RippleSettings = {
  brush: "ring",
  speed: 14,
  thickness: 1.15,
  lifetime: 1.15,
  fadePower: 1.35,
  echoCount: 1,
  echoDelay: 0.12,
  brightness: 1,
  idle: { r: 6, g: 8, b: 10 },
  colorMode: "rainbow",
  solid: { r: 46, g: 230, b: 214 },
  impactFlash: true,
  impactHold: 0.08,
  blend: "max",
};

export interface Ripple {
  x: number;
  y: number;
  t0: number;
  color: RGB;
  echo: number;
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

export function colorForPress(
  settings: RippleSettings,
  now: number,
  seed: number,
): RGB {
  if (settings.colorMode === "solid") return settings.solid;
  if (settings.colorMode === "rainbow") {
    return hsvToRgb(((now * 0.12 + seed * 0.17) % 1 + 1) % 1, 0.82, 1);
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
): Ripple[] {
  const color = colorForPress(settings, now, seed);
  const out: Ripple[] = [{ x, y, t0: now, color, echo: 0 }];
  for (let i = 1; i <= settings.echoCount; i++) {
    out.push({
      x,
      y,
      t0: now + i * settings.echoDelay,
      color,
      echo: i,
    });
  }
  return out;
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

  for (const ripple of ripples) {
    const elapsed = now - ripple.t0;
    if (elapsed < 0 || elapsed > settings.lifetime) continue;

    const radius = settings.speed * elapsed;
    const dx = x - ripple.x;
    const dy = y - ripple.y;
    const dist = Math.hypot(dx, dy);
    const fade = Math.pow(
      Math.max(0, 1 - elapsed / settings.lifetime),
      settings.fadePower,
    );

    let intensity = 0;
    if (settings.brush === "ring") {
      const band = Math.abs(dist - radius);
      intensity = Math.max(0, 1 - band / Math.max(0.05, settings.thickness));
    } else if (settings.brush === "fill") {
      if (dist <= radius) {
        intensity = 1 - dist / Math.max(radius, 0.001);
      }
    } else {
      const sigma = Math.max(0.15, settings.thickness * 0.85);
      const d = dist - radius;
      intensity = Math.exp(-(d * d) / (2 * sigma * sigma));
    }

    if (settings.impactFlash && dist < 0.55) {
      const hold = settings.impactHold;
      const flash =
        elapsed < hold
          ? 1
          : Math.max(0, 1 - (elapsed - hold) / (settings.lifetime * 0.35));
      intensity = Math.max(intensity, flash);
    }

    intensity *= fade * settings.brightness;
    if (intensity <= 0.002) continue;

    if (settings.blend === "add") {
      r = Math.min(255, r + ripple.color.r * intensity);
      g = Math.min(255, g + ripple.color.g * intensity);
      b = Math.min(255, b + ripple.color.b * intensity);
    } else {
      const keep = 1 - intensity;
      r = ripple.color.r * intensity + r * keep;
      g = ripple.color.g * intensity + g * keep;
      b = ripple.color.b * intensity + b * keep;
    }
  }

  return { r: clampByte(r), g: clampByte(g), b: clampByte(b) };
}

export function pruneRipples(ripples: Ripple[], now: number, lifetime: number) {
  let w = 0;
  for (let i = 0; i < ripples.length; i++) {
    const ripple = ripples[i];
    if (now - ripple.t0 <= lifetime) {
      ripples[w++] = ripple;
    }
  }
  ripples.length = w;
}

export function rgbCss(c: RGB, a = 1): string {
  return `rgba(${clampByte(c.r)},${clampByte(c.g)},${clampByte(c.b)},${a})`;
}
