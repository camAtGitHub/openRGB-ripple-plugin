import { RotateCcw } from "lucide-react";
import { Button } from "@/components/ui/button";
import { Slider } from "@/components/ui/slider";
import { Switch } from "@/components/ui/switch";
import type { BrushType, ColorMode } from "@/lib/ripple";
import { useStudio } from "@/lib/store";
import { cn } from "@/lib/utils";

function Row({
  label,
  value,
  children,
}: {
  label: string;
  value?: string;
  children: React.ReactNode;
}) {
  return (
    <div className="grid gap-2">
      <div className="flex items-baseline justify-between gap-3">
        <span className="text-[0.7rem] font-medium uppercase tracking-[0.14em] text-fg-subtle">
          {label}
        </span>
        {value ? (
          <span className="font-mono text-[0.68rem] tabular-nums text-fg-muted">
            {value}
          </span>
        ) : null}
      </div>
      {children}
    </div>
  );
}

function Segment<T extends string>({
  value,
  onChange,
  options,
}: {
  value: T;
  onChange: (v: T) => void;
  options: { id: T; label: string }[];
}) {
  return (
    <div className="flex rounded-sm bg-bg-subtle p-0.5">
      {options.map((opt) => (
        <button
          key={opt.id}
          type="button"
          onClick={() => onChange(opt.id)}
          className={cn(
            "h-8 flex-1 rounded-[6px] text-[0.7rem] font-medium transition-colors",
            value === opt.id
              ? "bg-bg-elevated text-fg"
              : "text-fg-muted hover:text-fg",
          )}
        >
          {opt.label}
        </button>
      ))}
    </div>
  );
}

function rgbToHex(r: number, g: number, b: number) {
  return (
    "#" +
    [r, g, b]
      .map((n) => Math.max(0, Math.min(255, n | 0)).toString(16).padStart(2, "0"))
      .join("")
  );
}

export function Controls() {
  const settings = useStudio((s) => s.settings);
  const set = useStudio((s) => s.set);
  const setBrush = useStudio((s) => s.setBrush);
  const setColorMode = useStudio((s) => s.setColorMode);
  const setSolidHex = useStudio((s) => s.setSolidHex);
  const reset = useStudio((s) => s.reset);
  const running = useStudio((s) => s.running);
  const demo = useStudio((s) => s.demo);
  const toggleRunning = useStudio((s) => s.toggleRunning);
  const toggleDemo = useStudio((s) => s.toggleDemo);
  const presses = useStudio((s) => s.presses);

  return (
    <aside className="flex w-full shrink-0 flex-col gap-6 rounded-xl border border-border bg-bg-panel p-5 sm:p-6 lg:w-[22rem]">
      <div className="flex items-start justify-between gap-3">
        <div>
          <h2 className="font-display text-lg font-semibold tracking-tight">
            Brush
          </h2>
          <p className="mt-1 text-sm text-fg-muted">
            Artemis Key Press — wave, fill, or soft bloom.
          </p>
        </div>
        <Button variant="ghost" size="icon" onClick={reset} aria-label="Reset">
          <RotateCcw className="size-4" />
        </Button>
      </div>

      <Segment<BrushType>
        value={settings.brush}
        onChange={setBrush}
        options={[
          { id: "ring", label: "Ring" },
          { id: "fill", label: "Fill" },
          { id: "soft", label: "Soft" },
        ]}
      />

      <Segment<ColorMode>
        value={settings.colorMode}
        onChange={setColorMode}
        options={[
          { id: "rainbow", label: "Rainbow" },
          { id: "solid", label: "Solid" },
          { id: "random", label: "Random" },
        ]}
      />

      {settings.colorMode === "solid" ? (
        <Row label="Color">
          <label className="flex h-10 items-center gap-3 rounded-sm border border-border bg-bg-elevated px-3">
            <input
              type="color"
              className="size-6 cursor-pointer rounded-xs border-0 bg-transparent"
              value={rgbToHex(settings.solid.r, settings.solid.g, settings.solid.b)}
              onChange={(e) => setSolidHex(e.target.value)}
            />
            <span className="font-mono text-xs text-fg-muted">
              {rgbToHex(settings.solid.r, settings.solid.g, settings.solid.b)}
            </span>
          </label>
        </Row>
      ) : null}

      <Row label="Speed" value={`${settings.speed.toFixed(1)} u/s`}>
        <Slider
          min={4}
          max={28}
          step={0.5}
          value={[settings.speed]}
          onValueChange={([v]) => set({ speed: v })}
        />
      </Row>
      <Row label="Thickness" value={settings.thickness.toFixed(2)}>
        <Slider
          min={0.35}
          max={3}
          step={0.05}
          value={[settings.thickness]}
          onValueChange={([v]) => set({ thickness: v })}
        />
      </Row>
      <Row label="Lifetime" value={`${settings.lifetime.toFixed(2)} s`}>
        <Slider
          min={0.35}
          max={2.8}
          step={0.05}
          value={[settings.lifetime]}
          onValueChange={([v]) => set({ lifetime: v })}
        />
      </Row>
      <Row label="Fade" value={settings.fadePower.toFixed(2)}>
        <Slider
          min={0.6}
          max={3}
          step={0.05}
          value={[settings.fadePower]}
          onValueChange={([v]) => set({ fadePower: v })}
        />
      </Row>
      <Row label="Echoes" value={String(settings.echoCount)}>
        <Slider
          min={0}
          max={4}
          step={1}
          value={[settings.echoCount]}
          onValueChange={([v]) => set({ echoCount: v })}
        />
      </Row>
      <Row label="Brightness" value={`${Math.round(settings.brightness * 100)}%`}>
        <Slider
          min={0.15}
          max={1}
          step={0.01}
          value={[settings.brightness]}
          onValueChange={([v]) => set({ brightness: v })}
        />
      </Row>

      <div className="flex items-center justify-between gap-3">
        <span className="text-sm text-fg-muted">Impact flash</span>
        <Switch
          checked={settings.impactFlash}
          onCheckedChange={(v) => set({ impactFlash: v })}
        />
      </div>
      <div className="flex items-center justify-between gap-3">
        <span className="text-sm text-fg-muted">Effect armed</span>
        <Switch checked={running} onCheckedChange={toggleRunning} />
      </div>
      <div className="flex items-center justify-between gap-3">
        <span className="text-sm text-fg-muted">Auto demo</span>
        <Switch checked={demo} onCheckedChange={toggleDemo} />
      </div>

      <p className="font-mono text-[0.68rem] tabular-nums text-fg-subtle">
        {presses} key events this session
      </p>
    </aside>
  );
}
