import { Download, FolderInput, Keyboard, Monitor } from "lucide-react";
import { Button } from "@/components/ui/button";

const STEPS = [
  {
    icon: Monitor,
    title: "OpenRGB 1.0rc3",
    body: "Confirm Plugin API 4 and SDK 5 in Settings → About. Enable the SDK server (port 6742) if you use the standalone client.",
  },
  {
    icon: FolderInput,
    title: "Install the plugin",
    body: "Settings → Plugins → Install plugin, then pick OpenRGBRipplePlugin.dll. Or copy it to %APPDATA%\\OpenRGB\\plugins and restart.",
  },
  {
    icon: Keyboard,
    title: "Put devices in Direct",
    body: "The plugin switches assigned keyboards to Direct mode, then paints an Artemis-style wave from each physical key press.",
  },
];

export function InstallPanel() {
  return (
    <section className="mx-auto grid w-full max-w-4xl gap-6">
      <div className="rounded-xl border border-border bg-bg-panel p-6 sm:p-8">
        <p className="font-mono text-[0.68rem] uppercase tracking-[0.18em] text-accent">
          Windows 11 · Plugin API 4
        </p>
        <h2 className="mt-2 font-display text-2xl font-semibold tracking-tight">
          OpenRGBRipplePlugin.dll
        </h2>
        <p className="mt-3 max-w-2xl text-sm leading-relaxed text-fg-muted">
          A dedicated OpenRGB plugin — same class of DLL as OpenRGBEffectsPlugin —
          that listens for key-down and expands a ripple from that LED. Built for
          OpenRGB 0.9+ / 1.0rc3 (SDK 5, Plugin API 4, Qt 5.15).
        </p>
        <div className="mt-6 flex flex-wrap gap-3">
          <Button asChild variant="accent">
            <a href="https://github.com/camAtGitHub/openRGB-ripple-plugin/releases/latest/download/OpenRGBRipplePlugin.dll">
              <Download className="size-4" />
              Download OpenRGBRipplePlugin.dll
            </a>
          </Button>
          <Button asChild variant="outline">
            <a href="https://github.com/camAtGitHub/openRGB-ripple-plugin">
              Source on GitHub
            </a>
          </Button>
        </div>
      </div>

      <ol className="grid gap-3 sm:grid-cols-3">
        {STEPS.map((step, i) => (
          <li
            key={step.title}
            className="rounded-lg border border-border bg-bg-panel p-5"
          >
            <step.icon className="size-4 text-accent" />
            <p className="mt-4 font-mono text-[0.65rem] text-fg-subtle">
              0{i + 1}
            </p>
            <h3 className="mt-1 font-display text-base font-semibold">
              {step.title}
            </h3>
            <p className="mt-2 text-sm leading-relaxed text-fg-muted">
              {step.body}
            </p>
          </li>
        ))}
      </ol>

      <div className="rounded-lg border border-border bg-bg-panel p-5 sm:p-6">
        <h3 className="font-display text-base font-semibold">Build the OpenRGB plugin DLL</h3>
        <p className="mt-2 text-sm leading-relaxed text-fg-muted">
          This is the real in-app plugin (Ripple tab + config panel). The
          script fetches Qt 5.15 and OpenRGB headers, builds
          <span className="font-mono text-fg"> OpenRGBRipplePlugin.dll</span>,
          and copies it into OpenRGB’s plugins folder. Needs VS 2022 C++
          workload and Python 3 the first time.
        </p>
        <pre className="mt-4 overflow-x-auto rounded-md bg-bg p-4 font-mono text-[0.72rem] leading-relaxed text-fg-muted">
{`cd OpenRGBRipplePlugin
powershell -ExecutionPolicy Bypass -File .\\build-plugin.ps1
# restart OpenRGB — look for the Ripple tab`}
        </pre>
        <p className="mt-4 text-sm leading-relaxed text-fg-muted">
          Close <span className="font-mono text-fg">OpenRGBRipple.exe</span>{" "}
          first so it does not fight the plugin. If the tab is missing:
          Settings → Plugins and enable it.
        </p>
      </div>
    </section>
  );
}
