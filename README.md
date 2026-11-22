# Ripple

Artemis-style key-press ripple plugin for [OpenRGB](https://openrgb.org).  
Windows · **Requires OpenRGB release 1.1** (Plugin API v5, Qt 6)

<p align="center">
  <img src="public/og.jpg" alt="Ripple expanding from a key on a backlit keyboard" width="860">
</p>

Rippling wave effects across your keyboard as you type. Choose style: ring, fill, or soft and colours of: rainbow, solid, or random. 
New features include idle timeout to enable key-saver (keyboard screensaver) and auto power-off. 
Single DLL installation integrated into openRGB's tabbed menu. 

**[Download OpenRGBRipplePlugin.dll](https://github.com/camAtGitHub/openRGB-ripple-plugin/releases/latest/download/OpenRGBRipplePlugin.dll)**

The plugin installs a Windows keyboard-hook, which may trigger an antivirus false positive. No issues with Microsoft Defender at present.

## Installation

Requires **OpenRGB release 1.1** (Plugin API v5). OpenRGB 1.0 and the 1.0 release candidates use Plugin API v4 and will not load this DLL (see older releases for v4 compatible).

Download the dll from the [releases](https://github.com/camAtGitHub/openRGB-ripple-plugin/releases) page and copy it to `%APPDATA%\OpenRGB\plugins`. Restart OpenRGB and open the **Ripple** tab.

## Web preview

Play with the demo on the [live preview](https://camatgithub.github.io/openRGB-ripple-plugin/).

<p align="center">
  <a href="https://camatgithub.github.io/openRGB-ripple-plugin/">
    <img src="docs/studio.jpg" alt="Open the Ripple studio — virtual keyboard and brush controls" width="860">
  </a>
</p>

## Idle Timeouts

If you have the supported [effects plugin](https://openrgb.org/plugin_effects.html) installed, you get the option 'Idle Timeout (enable effects)' in the **Ripple** tab. This will automatically enable the effects plugin after a period of inactivity, and switch back to Ripple when you start typing again. To configure it, go to the **Effects** tab and select the effect you want to use (I use shaders personally) when idle but set them to 'stopped'. Then go back to the **Ripple** tab and set the idle timeout duration. Make sure to save your profile with both effects and ripple states included.

## Manually building the plugin

You don't need to to use the plugin (See Installation above).

Needs Visual Studio 2022 (**Desktop development with C++**) and Python 3 the first time (to fetch Qt 6.8.3).

```powershell
cd OpenRGBRipplePlugin
powershell -ExecutionPolicy Bypass -File .\build-plugin.ps1
```

That builds `OpenRGBRipplePlugin.dll` and copies it to `%APPDATA%\OpenRGB\plugins`. Restart OpenRGB and open the **Ripple** tab. Pass `-NoInstall` to skip the copy (CI / just produce the DLL).

Details, build instructions and the config table: [`OpenRGBRipplePlugin/README.md`](OpenRGBRipplePlugin/README.md).

## License

GPL-2.0-or-later, same as OpenRGB.

## This repo

Repo: [camAtGitHub/openRGB-ripple-plugin](https://github.com/camAtGitHub/openRGB-ripple-plugin).  

## Inspired by

The effect is inspired by [Artemis 2's](https://github.com/Artemis-RGB/Artemis) Ripple brush effect.

