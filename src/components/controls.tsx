import { RotateCcw } from "lucide-react";
import { Button } from "@/components/ui/button";
import { Slider } from "@/components/ui/slider";
import { Switch } from "@/components/ui/switch";
import {
  BLAST_SHAPE_OPTIONS,
  BLEND_OPTIONS,
  SHAPE_OPTIONS,
  type BlastShape,
  type BrushType,
  type ColorMode,
  type WaveShape,
} from "@/lib/ripple";
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
  columns,
}: {
  value: T;
  onChange: (v: T) => void;
  options: { id: T; label: string }[];
  columns?: 2 | 4;
}) {
  return (
    <div
      className={cn(
        "rounded-sm bg-bg-subtle p-0.5",
        columns === 2 ? "grid grid-cols-2 gap-0.5" : "flex",
      )}
    >
      {options.map((opt) => (
        <button
          key={opt.id}
          type="button"
          onClick={() => onChange(opt.id)}
          className={cn(
            "h-8 rounded-[6px] text-[0.7rem] font-medium transition-colors",
            columns === 2 ? "px-2" : "flex-1",
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
  const setShape = useStudio((s) => s.setShape);
  const setColorMode = useStudio((s) => s.setColorMode);
  const setSolidHex = useStudio((s) => s.setSolidHex);
  const setIdleHex = useStudio((s) => s.setIdleHex);
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
            {settings.shape === "square"
              ? "Square front — same brushes, Chebyshev distance."
              : settings.shape === "axis"
                ? "Pulse along one row or column. Direction takes the longer remaining path."
                : settings.shape === "sweep"
                  ? "Full row or column as a bar, then it slides the long way."
                  : settings.shape === "jump"
                    ? "Dart flies last key → this key. One explosion on the last key after you pause."
                    : "Artemis Key Press — wave, fill, or soft bloom."}
          </p>
        </div>
        <Button variant="ghost" size="icon" onClick={reset} aria-label="Reset">
          <RotateCcw className="size-4" />
        </Button>
      </div>

      <Row label="Shape">
        <Segment<WaveShape>
          value={settings.shape}
          onChange={setShape}
          options={SHAPE_OPTIONS}
          columns={2}
        />
      </Row>

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

      <Row label="Background">
        <label className="flex h-10 items-center gap-3 rounded-sm border border-border bg-bg-elevated px-3">
          <input
            type="color"
            className="size-6 cursor-pointer rounded-xs border-0 bg-transparent"
            value={rgbToHex(settings.idle.r, settings.idle.g, settings.idle.b)}
            onChange={(e) => setIdleHex(e.target.value)}
          />
          <span className="font-mono text-xs text-fg-muted">
            {rgbToHex(settings.idle.r, settings.idle.g, settings.idle.b)}
          </span>
        </label>
      </Row>

      <Row label="Blend">
        <div className="grid grid-cols-4 gap-0.5 rounded-sm bg-bg-subtle p-0.5">
          {BLEND_OPTIONS.map((opt) => (
            <button
              key={opt.id}
              type="button"
              onClick={() => set({ blend: opt.id })}
              className={cn(
                "h-8 rounded-[6px] px-1 text-[0.62rem] font-medium transition-colors",
                settings.blend === opt.id
                  ? "bg-bg-elevated text-fg"
                  : "text-fg-muted hover:text-fg",
              )}
            >
              {opt.label}
            </button>
          ))}
        </div>
      </Row>

      {settings.shape === "axis" || settings.shape === "sweep" ? (
        <Row
          label="Jitter"
          value={`${Math.round(settings.axisJitter * 100)}%`}
        >
          <Slider
            min={0}
            max={1}
            step={0.01}
            value={[settings.axisJitter]}
            onValueChange={([v]) => set({ axisJitter: v })}
          />
          <p className="text-[0.68rem] leading-snug text-fg-subtle">
            {settings.shape === "sweep"
              ? "0% the bar always slides the long way (numpad 0 → left or up). 100% coin-flips."
              : "0% always runs the long way (max time on). 100% coin-flips direction on that row or column."}
          </p>
        </Row>
      ) : null}

      {settings.shape === "jump" ? (
        <Row
          label="Trail"
          value={`${settings.trailLength.toFixed(1)} keys`}
        >
          <Slider
            min={0}
            max={16}
            step={0.25}
            value={[settings.trailLength]}
            onValueChange={([v]) => set({ trailLength: v })}
          />
          <p className="text-[0.68rem] leading-snug text-fg-subtle">
            0 = only the moving blob. Higher = longer comet behind it.
          </p>
        </Row>
      ) : null}

      {settings.shape === "jump" ? (
        <Row label="Explosion">
          <Segment<BlastShape>
            value={settings.blastShape}
            onChange={(v) => set({ blastShape: v })}
            options={BLAST_SHAPE_OPTIONS}
          />
        </Row>
      ) : null}

      {settings.shape === "jump" ? (
        <Row
          label="Blast size"
          value={`${settings.blastSize.toFixed(1)} keys`}
        >
          <Slider
            min={0.5}
            max={12}
            step={0.25}
            value={[settings.blastSize]}
            onValueChange={([v]) => set({ blastSize: v })}
          />
        </Row>
      ) : null}

      {settings.shape === "sweep" ? (
        <Row
          label="Span"
          value={`${Math.round(settings.sweepSpan * 100)}%`}
        >
          <Slider
            min={0}
            max={1}
            step={0.01}
            value={[settings.sweepSpan]}
            onValueChange={([v]) => set({ sweepSpan: v })}
          />
          <p className="text-[0.68rem] leading-snug text-fg-subtle">
            How much of the axis lights. 100% is a full row or column bar.
          </p>
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
          min={0.05}
          max={2.8}
          step={0.05}
          value={[settings.lifetime]}
          onValueChange={([v]) => set({ lifetime: v })}
        />
        {settings.shape === "sweep" || settings.shape === "axis" ? (
          <p className="text-[0.68rem] leading-snug text-fg-subtle">
            Time for the bar to reach the far edge. Fade is extra, after that.
          </p>
        ) : settings.shape === "jump" ? (
          <p className="text-[0.68rem] leading-snug text-fg-subtle">
            Idle after the last key, then one boom there. Turn Auto demo off or it never idles.
          </p>
        ) : null}
      </Row>
      <Row label="Fade" value={`${settings.fade.toFixed(2)} s`}>
        <Slider
          min={0}
          max={3}
          step={0.05}
          value={[settings.fade]}
          onValueChange={([v]) => set({ fade: v })}
        />
        <p className="text-[0.68rem] leading-snug text-fg-subtle">
          {settings.shape === "jump"
            ? "Explosion retract. 0 s snaps the blast off."
            : "Retract from the outer edge back to the key. 0 s snaps off."}
        </p>
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
