# OpenRGB Ripple Plugin

Windows plugin for [OpenRGB](https://openrgb.org) **1.0rc3**
(Plugin API **4**, SDK **5**, Qt **5.15**).

The **Ripple** tab inside OpenRGB holds the config panel.

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

An alternative to the plugin is `OpenRGBRipple.exe` in `sdk-client\`. It uses the OpenRGB SDK to talk to the server and paint the keyboard.
To use it start openRGB and enabled the SDK Server (tab) on port 6742. Then in a different console run `OpenRGBRipple.exe` (or `OpenRGBRipple.exe -help` for help).

If you only want the exe, see `sdk-client\build-msvc.bat`.
That talks to the SDK server; it is **not** an OpenRGB plugin.
Do **not** run `OpenRGBRipple.exe` at the same time — both would paint the keyboard.

## License

GPL-2.0 (same as OpenRGB).
