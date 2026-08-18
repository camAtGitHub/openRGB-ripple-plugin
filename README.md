# Ripple

Artemis-style key-press ripple plugin for [OpenRGB](https://openrgb.org).  
Compatible with Windows · OpenRGB Plugin API v4 · OpenRGB 1.0rc2 / 1.0rc3

<p align="center">
  <img src="public/og.jpg" alt="Ripple expanding from a key on a backlit keyboard" width="860">
</p>

Rippling wave effects across your keyboard as you type. Choose style: ring, fill, or soft and colours of: rainbow, solid, or random. 
Single DLL installation integrated into openRGB's tabbed menu. 

**[Download OpenRGBRipplePlugin.dll](https://github.com/camAtGitHub/openRGB-ripple-plugin/releases/latest/download/OpenRGBRipplePlugin.dll)**

The plugin installs a Windows keyboard-hook, which may trigger an antivirus false positive. No issues with Microsoft Defender at present.

## Installation

Download the dll from the [releases](https://github.com/camAtGitHub/openRGB-ripple-plugin/releases) page and copy it to `%APPDATA%\OpenRGB\plugins`. Restart OpenRGB and open the **Ripple** tab.

## Web preview

Play with the demo on the [live preview](https://camatgithub.github.io/openRGB-ripple-plugin/).

<p align="center">
  <a href="https://camatgithub.github.io/openRGB-ripple-plugin/">
    <img src="docs/studio.jpg" alt="Open the Ripple studio — virtual keyboard and brush controls" width="860">
  </a>
</p>

## Build the plugin

Needs Visual Studio 2022 (**Desktop development with C++**) and Python 3 the first time (to fetch Qt 5.15).

```powershell
cd OpenRGBRipplePlugin
powershell -ExecutionPolicy Bypass -File .\build-plugin.ps1
```

That builds `OpenRGBRipplePlugin.dll` and copies it to `%APPDATA%\OpenRGB\plugins`. Restart OpenRGB and open the **Ripple** tab. Pass `-NoInstall` to skip the copy (CI / just produce the DLL).

Details, build instructions and the config table: [`OpenRGBRipplePlugin/README.md`](OpenRGBRipplePlugin/README.md).

Do **not** run `OpenRGBRipple.exe` at the same time — both would paint the keyboard.

A standalone SDK client (no Qt) lives in [`OpenRGBRipplePlugin/sdk-client/`](OpenRGBRipplePlugin/sdk-client/).

## License

GPL-2.0-or-later, same as OpenRGB.

## This repo

Repo: [camAtGitHub/openRGB-ripple-plugin](https://github.com/camAtGitHub/openRGB-ripple-plugin).  

## Inspired by

The effect is inspired by [Artemis 2's](https://github.com/Artemis-RGB/Artemis) Ripple brush effect.

