# OpenRGB Ripple Plugin

Windows plugin for [OpenRGB](https://openrgb.org) **1.0rc3**
(Plugin API **4**, SDK **5**, Qt **5.15**).

When you press a key, a ripple expands from that LED — Artemis 2
**Key Press** (ring / fill / soft). A **Ripple** tab inside OpenRGB
holds the config panel.

## Build the DLL (one script)

Needs Visual Studio 2022 (**Desktop development with C++**) and
Python 3 on PATH the first time (to download Qt 5.15).

```powershell
cd OpenRGBRipplePlugin
powershell -ExecutionPolicy Bypass -File .\build-plugin.ps1
# or, just produce the DLL (no %APPDATA% copy):
powershell -ExecutionPolicy Bypass -File .\build-plugin.ps1 -NoInstall
```

The script:

1. Clones OpenRGB `release_candidate_1.0rc2` (headers only)
2. Installs Qt 5.15.2 msvc2019_64 into `.qt\` if you do not already have it
3. Builds `OpenRGBRipplePlugin.dll`
4. Copies it to `%APPDATA%\OpenRGB\plugins` unless you pass `-NoInstall`

Restart OpenRGB. A **Ripple** tab appears.

Do **not** run `OpenRGBRipple.exe` at the same time — both would paint the keyboard.

## Manual install

If the script built the DLL but did not copy it:

- OpenRGB → **Settings → Plugins → Install plugin** → pick `build\OpenRGBRipplePlugin.dll`
- or copy it to `%APPDATA%\OpenRGB\plugins` and restart

## Config panel

| Control | What it does |
| --- | --- |
| Enabled | Arm the effect |
| Brush | Ring / Fill / Soft |
| Color | Rainbow / Solid / Random |
| Wave | Solid wave colour (click the swatch) |
| Background | Colour of keys the wave is not on |
| Blend | Over (replace background) or Add (stack glow) |
| Disable background | Leave unused keys alone for another effect |
| Speed, thickness, lifetime, fade, echoes, brightness | Same as the SDK client |
| Keyboards | Which devices to paint |
| Tray | Enable / Disable |

Settings persist in OpenRGB’s settings store.

## Standalone SDK client (no Qt)

If you only want the exe, see `sdk-client\build-msvc.bat`.
That talks to the SDK server; it is **not** an OpenRGB plugin.

## License

GPL-2.0-or-later (same as OpenRGB).
