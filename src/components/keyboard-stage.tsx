import { useEffect, useMemo, useRef, useState } from "react";
import {
  KEYBOARD,
  KEY_BY_CODE,
  LAYOUT_HEIGHT,
  LAYOUT_WIDTH,
  keyCenter,
  type KeySpec,
} from "@/lib/keyboard-layout";
import {
  pruneRipples,
  rgbCss,
  sampleRipples,
  spawnRipples,
  type RGB,
  type Ripple,
  type RippleSettings,
} from "@/lib/ripple";
import { useStudio } from "@/lib/store";
import { cn } from "@/lib/utils";

const DEMO_SEQ = [
  "KeyA",
  "KeyS",
  "KeyD",
  "KeyF",
  "KeyJ",
  "KeyK",
  "KeyL",
  "Space",
  "Enter",
  "KeyW",
  "ArrowUp",
  "KeyQ",
];

function KeyCap({
  spec,
  color,
  unit,
}: {
  spec: KeySpec;
  color: RGB;
  unit: number;
}) {
  const glow = `0 0 ${Math.max(6, unit * 0.35)}px ${rgbCss(color, 0.55)}`;
  const face = rgbCss(color, 0.22);
  const edge = rgbCss(color, 0.55);
  return (
    <button
      type="button"
      data-key={spec.id}
      aria-label={spec.label || spec.id}
      className="absolute overflow-hidden rounded-[5px] border text-left outline-none focus-visible:ring-1 focus-visible:ring-accent"
      style={{
        left: spec.x * unit,
        top: spec.row * unit,
        width: spec.w * unit - 3,
        height: (spec.h ?? 1) * unit - 3,
        background: `linear-gradient(180deg, ${face}, rgb(10 10 12 / 0.92))`,
        borderColor: edge,
        boxShadow: glow,
      }}
    >
      <span
        className="pointer-events-none absolute inset-x-0 top-0 h-1/2 opacity-30"
        style={{
          background:
            "linear-gradient(180deg, rgb(255 255 255 / 0.16), transparent)",
        }}
      />
      <span className="relative flex h-full flex-col justify-between px-[18%] py-[14%]">
        {spec.sub ? (
          <span className="font-display text-[0.55rem] leading-none text-fg/80 sm:text-[0.62rem]">
            {spec.sub}
          </span>
        ) : (
          <span />
        )}
        <span
          className={cn(
            "font-display leading-none text-fg",
            spec.w >= 1.5 ? "text-[0.58rem] sm:text-[0.7rem]" : "text-[0.62rem] sm:text-[0.75rem]",
          )}
        >
          {spec.label}
        </span>
      </span>
    </button>
  );
}

export function KeyboardStage() {
  const settings = useStudio((s) => s.settings);
  const running = useStudio((s) => s.running);
  const demo = useStudio((s) => s.demo);
  const bumpPress = useStudio((s) => s.bumpPress);

  const wrapRef = useRef<HTMLDivElement>(null);
  const ripplesRef = useRef<Ripple[]>([]);
  const settingsRef = useRef<RippleSettings>(settings);
  const runningRef = useRef(running);
  const seedRef = useRef(0);
  const [unit, setUnit] = useState(28);
  const [colors, setColors] = useState<Record<string, RGB>>({});

  settingsRef.current = settings;
  runningRef.current = running;

  useEffect(() => {
    const el = wrapRef.current;
    if (!el) return;
    const ro = new ResizeObserver(() => {
      const w = el.clientWidth;
      setUnit(Math.max(14, Math.min(36, w / LAYOUT_WIDTH)));
    });
    ro.observe(el);
    setUnit(Math.max(14, Math.min(36, el.clientWidth / LAYOUT_WIDTH)));
    return () => ro.disconnect();
  }, []);

  const fire = (key: KeySpec, now = performance.now() / 1000) => {
    if (!runningRef.current) return;
    seedRef.current += 1;
    const c = keyCenter(key);
    ripplesRef.current.push(
      ...spawnRipples(settingsRef.current, c.x, c.y, now, seedRef.current),
    );
    bumpPress();
  };

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.repeat || e.metaKey) return;
      const key = KEY_BY_CODE.get(e.code);
      if (!key) return;
      if (
        e.target instanceof HTMLElement &&
        (e.target.tagName === "INPUT" || e.target.tagName === "TEXTAREA")
      ) {
        return;
      }
      e.preventDefault();
      fire(key);
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  useEffect(() => {
    if (!demo || !running) return;
    let i = 0;
    const id = window.setInterval(() => {
      const code = DEMO_SEQ[i % DEMO_SEQ.length];
      const key = KEY_BY_CODE.get(code);
      if (key) fire(key);
      i += 1;
    }, 520);
    return () => window.clearInterval(id);
  }, [demo, running]);

  useEffect(() => {
    let raf = 0;
    const tick = () => {
      const now = performance.now() / 1000;
      const s = settingsRef.current;
      pruneRipples(ripplesRef.current, now, s.lifetime + s.echoCount * s.echoDelay);
      const next: Record<string, RGB> = {};
      for (const key of KEYBOARD) {
        const c = keyCenter(key);
        next[key.id] = sampleRipples(s, ripplesRef.current, c.x, c.y, now);
      }
      setColors(next);
      raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(raf);
  }, []);

  const height = unit * LAYOUT_HEIGHT;
  const width = unit * LAYOUT_WIDTH;

  const onClickKey = (e: React.MouseEvent<HTMLDivElement>) => {
    const btn = (e.target as HTMLElement).closest("[data-key]");
    if (!btn) return;
    const id = btn.getAttribute("data-key");
    const key = KEYBOARD.find((k) => k.id === id);
    if (key) fire(key);
  };

  const liveCount = useMemo(
    () => ripplesRef.current.length,
    [colors],
  );

  return (
    <div className="flex min-w-0 flex-1 flex-col">
      <div
        ref={wrapRef}
        className="relative w-full overflow-x-auto overflow-y-hidden"
      >
        <div
          className="relative mx-auto"
          style={{ width, height }}
          onClick={onClickKey}
        >
          {KEYBOARD.map((key) => (
            <KeyCap
              key={key.id}
              spec={key}
              unit={unit}
              color={colors[key.id] ?? settings.idle}
            />
          ))}
        </div>
      </div>
      <p className="mt-4 hidden text-center font-mono text-[0.68rem] tracking-wide text-fg-subtle sm:block">
        {liveCount} active waves · type or tap a key
      </p>
    </div>
  );
}
