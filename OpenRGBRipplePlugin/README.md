# OpenRGB Ripple Plugin

Windows plugin for [OpenRGB](https://openrgb.org).
Requires OpenRGB release **1.1** (Plugin API **v5**, Qt **6.8**).
OpenRGB 1.0 and the 1.0 release candidates (Plugin API v4) will not load it.

The **Ripple** tab inside OpenRGB holds the config panel.

## Build the DLL (one script)

Needs Visual Studio 2022 (**Desktop development with C++**) and
Python 3 on PATH the first time (to download Qt 6.8.3).

```powershell
cd OpenRGBRipplePlugin
powershell -ExecutionPolicy Bypass -File .\build-plugin.ps1
# or, just produce the DLL (no %APPDATA% copy):
powershell -ExecutionPolicy Bypass -File .\build-plugin.ps1 -NoInstall
```

The script:

1. Clones OpenRGB `release_1.1` (headers only)
2. Installs Qt 6.8.3 msvc2022_64 into `.qt\` if you do not already have it
3. Builds `OpenRGBRipplePlugin.dll`
4. Copies it to `%APPDATA%\OpenRGB\plugins` unless you pass `-NoInstall`

Restart OpenRGB. A **Ripple** tab appears.


## Manual install

If the script built the DLL but did not copy it:

- OpenRGB → **Settings → Plugins → Install plugin** → pick `build\OpenRGBRipplePlugin.dll`
- or copy it to `%APPDATA%\OpenRGB\plugins` and restart

## Config panel

| Control | What it does |
| --- | --- |
| Enabled | Arm the effect |
| Brush | Ring / Fill / Soft |
| Shape | Circle, Square, Row/Col, Sweep, Dart |
| Color | Rainbow / Solid / Random. Rainbow is saturated hue bands per key-width. |
| Wave | Solid wave colour (click the swatch) |
| Background | Colour of keys the wave is not on |
| Blend | Over, Add, XOR, Screen, Overlay, Color Dodge, Color Burn, Exclusion |
| Disable background | Leave unused keys alone for another effect |
| Speed | Wave speed, keys/sec |
| Thickness | Ring width |
| Lifetime | Max expand seconds (min 0.05). Sweep / Row-Col travel at Speed and may finish sooner. Dart: idle wait after the last keydown, then the boom expands for this long |
| Retract | Seconds to pull the front back to the key; 0 = snap off |
| Echoes, brightness | Extra rings and overall brightness |
| Jitter | Row/Col + Sweep: chance the wave takes the short way |
| Span | Sweep lateral width |
| Trail | Dart: keys lit behind the head. 0 = blob only |
| Explosion | Dart blast shape: Circle or Square |
| Blast size | Dart explosion radius in key-widths |
| Idle Timeout (enable effects) | Only shown when the Effects plugin is installed. Disabled or 1–60 min without a keypress → **Start All Effects** (Effects plugin) and Ripple steps aside. Skipped if Effects is not loaded. Next keypress stops Effects and Ripple resumes with your settings |
| Turn off Keyboard | Disabled or 1–120 min (must be longer than Idle Timeout; stepping up jumps past it) → **Stop All Effects**, then all keyboards go dark. Next keypress resumes Ripple |
| Keyboards | Which devices to paint |
| Tray | Enable / Disable |

Dart flies from the previous key to the one you just pressed. After lifetime seconds with no new key, one explosion fires at the last key and wipes in-flight darts. Pressing the same key twice only lights that key (never the whole board).

Settings persist in OpenRGB’s settings store.

## License

GPL-2.0 (same as OpenRGB).
