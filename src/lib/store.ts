import { create } from "zustand";
import {
  DEFAULT_SETTINGS,
  type BrushType,
  type ColorMode,
  type RippleSettings,
} from "./ripple";

type Patch = Partial<RippleSettings>;

interface StudioState {
  settings: RippleSettings;
  running: boolean;
  demo: boolean;
  presses: number;
  set: (patch: Patch) => void;
  setBrush: (brush: BrushType) => void;
  setColorMode: (colorMode: ColorMode) => void;
  setSolidHex: (hex: string) => void;
  reset: () => void;
  toggleRunning: () => void;
  toggleDemo: () => void;
  bumpPress: () => void;
}

function hexToRgb(hex: string) {
  const h = hex.replace("#", "");
  const n = parseInt(h.length === 3 ? h.split("").map((c) => c + c).join("") : h, 16);
  return { r: (n >> 16) & 255, g: (n >> 8) & 255, b: n & 255 };
}

export const useStudio = create<StudioState>((set) => ({
  settings: DEFAULT_SETTINGS,
  running: true,
  demo: true,
  presses: 0,
  set: (patch) => set((s) => ({ settings: { ...s.settings, ...patch } })),
  setBrush: (brush) => set((s) => ({ settings: { ...s.settings, brush } })),
  setColorMode: (colorMode) =>
    set((s) => ({ settings: { ...s.settings, colorMode } })),
  setSolidHex: (hex) =>
    set((s) => ({ settings: { ...s.settings, solid: hexToRgb(hex) } })),
  reset: () => set({ settings: DEFAULT_SETTINGS }),
  toggleRunning: () => set((s) => ({ running: !s.running })),
  toggleDemo: () => set((s) => ({ demo: !s.demo })),
  bumpPress: () => set((s) => ({ presses: s.presses + 1 })),
}));
