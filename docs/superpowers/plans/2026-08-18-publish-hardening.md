# OpenRGB Ripple Publish Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Isolation:** Use superpowers:using-git-worktrees. This plan is designed for **three parallel worktrees** and **parallel subagents**. Do not implement Tracks A/B/C in the same checkout.

**Goal:** Make the OpenRGB Ripple plugin publishable to Calc (OpenRGB maintainer) by deleting the remaining rescan/unload crash class: host callbacks must target the plugin `QObject`, cached `RGBController*` must live in a generation-counted session, settings must not throw, and the keyboard hook must be a real `HOOKPROC`.

**Architecture:** One `DeviceSession` owns every `RGBController*` and a generation counter. `OpenRGBRipplePlugin` is the only `void*` registered with ResourceManager. `RippleWidget` paints from a live snapshot and never appears in a host callback. `KeyboardHook` copies the SDK client's Win32 signature. Settings parse is a non-throwing helper.

**Tech Stack:** C++17, Qt 5.15, OpenRGB Plugin API 4 (`release_candidate_1.0rc2` headers), `WH_KEYBOARD_LL`, nlohmann `json` via OpenRGB `SettingsManager`.

**Release version after this work:** `1.0.2` (lifetime fix is not the same binary as the 1.0.1 80 ms band-aid).

---

## Execution model (worktrees + subagents)

```
                    main (read-only during Phase 1)
                         |
         +---------------+---------------+
         |               |               |
   worktree A      worktree B      worktree C
   fix/device-     fix/keyboard-   fix/version-
   session         hook            and-tests
   (subagent A)    (subagent B)    (subagent C)
         |               |               |
         +-------+-------+-------+-------+
                 |
          orchestrator merge
          branch: fix/publish-hardening
                 |
          Phase 3 verification (one worktree)
```

**Orchestrator (this or a later `/do` / execute-plan session) only:**

1. Adds `.worktrees/` to `.gitignore` and commits that first on `main` (or on `fix/publish-hardening` cut from `main`).
2. Creates three isolated worktrees (see Phase 1 Task 0).
3. Spawns **three `general-purpose` subagents in one turn**, each with `cwd` set to its worktree (or `isolation: "worktree"` if using spawn isolation). Each agent gets **only its track** from this file — paste the track, Phase 0 Allowed APIs, and anti-pattern guards. They must not edit files owned by another track.
4. Waits for all three. Rejects any agent that invented an API not in Phase 0.
5. Merges B, then C, then A onto `fix/publish-hardening` (A is the largest; merge last so hook/tests land cleanly).
6. Runs Phase 3 verification itself or with one reviewer subagent.

**File ownership (do not cross):**

| Track | Owns (create/modify) | Must not touch |
| --- | --- | --- |
| **A** `fix/device-session` | `DeviceSession.h`, `DeviceSession.cpp`, `RippleSettingsIO.h`, `OpenRGBRipplePlugin.h`, `OpenRGBRipplePlugin.cpp`, `RippleWidget.h`, `RippleWidget.cpp`, `CMakeLists.txt` (sources only), `OpenRGBRipplePlugin.pro` (SOURCES/HEADERS only) | `KeyboardHook.*`, version string literals except if a touched UI title must stay in sync — A sets the title from a shared `#define` introduced by C, or leaves `"Ripple 1.0.1"` and C's merge updates it |
| **B** `fix/keyboard-hook` | `KeyboardHook.h`, `KeyboardHook.cpp` | Everything else |
| **C** `fix/version-and-tests` | version literals in `.pro` / `CMakeLists.txt` / `OpenRGBRipplePlugin.cpp` / `RippleWidget.cpp` title, `tests/hostless/*`, `tests/CMakeLists.txt`, `.gitignore` test bits | lifetime logic, hook ABI |

**Merge conflict expected (one file):** `CMakeLists.txt` and `.pro` — A adds sources, C may add a test target and `VERSION`. Orchestrator resolves by keeping both hunks.

**Do not** implement this in the user's dirty `main` working tree. Untracked `public/OpenRGBRipplePlugin.zip` and `public/sdk-client/` stay out of every commit.

---

## Phase 0: Allowed APIs (frozen)

Discovery ran on 2026-08-18 against this repo plus OpenRGB `release_candidate_1.0rc2` headers. **Do not invent anything not listed here.**

### Plugin ABI (API 4 only)

Source: https://gitlab.com/CalcProgrammer1/OpenRGB/-/raw/release_candidate_1.0rc2/OpenRGBPluginInterface.h

```cpp
#define OpenRGBPluginInterface_IID  "com.OpenRGBPluginInterface"
#define OPENRGB_PLUGIN_API_VERSION  4

class OpenRGBPluginInterface
{
public:
    virtual                    ~OpenRGBPluginInterface() {}
    virtual OpenRGBPluginInfo   GetPluginInfo()                                      = 0;
    virtual unsigned int        GetPluginAPIVersion()                                = 0;
    virtual void                Load(ResourceManagerInterface* resource_manager_ptr) = 0;
    virtual QWidget*            GetWidget()                                          = 0;
    virtual QMenu*              GetTrayMenu()                                        = 0;
    virtual void                Unload()                                             = 0;
};
```

- Plugin class **must** stay `public QObject, public OpenRGBPluginInterface` with `Q_OBJECT`, `Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "OpenRGBRipplePlugin.json")`, `Q_INTERFACES`.
- **Master / API 5 is forbidden.** No `Load(OpenRGBPluginAPIInterface*)`, no `OnSDKCommand`, no `org.openrgb.OpenRGBPluginInterface`.
- Host calls `Load` then `GetWidget` **once per enable cycle**, and again after disable+re-enable (`OpenRGBDialog::AddPlugin`). `GetWidget` must be idempotent inside one Load cycle and must create a fresh widget after `Unload`.

### ResourceManagerInterface (plugin-facing)

Source: https://gitlab.com/CalcProgrammer1/OpenRGB/-/raw/release_candidate_1.0rc2/ResourceManagerInterface.h

**Allowed calls (this plugin already uses them):**

```cpp
void RegisterDeviceListChangeCallback(DeviceListChangeCallback, void*);
void RegisterDetectionProgressCallback(DetectionProgressCallback, void*);
void RegisterDetectionStartCallback(DetectionStartCallback, void*);
void RegisterDetectionEndCallback(DetectionEndCallback, void*);
void UnregisterDeviceListChangeCallback(DeviceListChangeCallback, void*);
void UnregisterDetectionProgressCallback(DetectionProgressCallback, void*);
void UnregisterDetectionStartCallback(DetectionStartCallback, void*);
void UnregisterDetectionEndCallback(DetectionEndCallback, void*);
std::vector<RGBController*>& GetRGBControllers();
unsigned int GetDetectionPercent();
SettingsManager* GetSettingsManager();
void WaitForDeviceDetection();
```

Callback type is **only** `typedef void (*…)(void*);`. The registered `void*` must be `OpenRGBRipplePlugin* this`, never `RippleWidget*`.

**Do not register `DetectionProgress`.** Progress is not teardown. Leave it unregistered (unregister in `Unload` if a previous build registered it).

**Does not exist on the interface — do not call:**

- Any lock / mutex / `DetectDeviceMutex` / `DeviceListChangeMutex`
- `RegisterClientInfoChangeCallback`
- `DetectDevices`, `RescanDevices`, `Cleanup`, `GetDetectionString`
- Concrete `ResourceManager.h` (do not include it)

**Host callback order** (from `ResourceManager.cpp` `ProcessPreDetection` / `Cleanup` / `ProcessPostDetection`):

1. `DetectionStart` callbacks
2. `Cleanup()` deletes every `RGBController` (`~RGBController` joins `DeviceCallThread`)
3. `UpdateDeviceList()` → `DeviceListChanged`
4. … detection …
5. `detection_percent = 100`, `DetectionProgressChanged`, then `DetectionEnd`

`RGBController::~RGBController` joins the USB worker. After the plugin stops calling `UpdateLEDs`/`UpdateMode`, **no sleep is required**.

`DeviceListChanged` also fires from `NetworkClientInfoChangeCallback` (SDK client) **without** a DetectionStart/End pair.

### SettingsManager

Source: rc2 `SettingsManager.h`

```cpp
json GetSettings(std::string settings_key);
void SetSettings(std::string settings_key, json new_settings);
void SaveSettings();
```

Key remains `"OpenRGBRipplePlugin"`. `json` is nlohmann. `.get<T>()` throws — must be caught inside our helper.

### Qt 5.15 only

Sources: qtbase 5.15 `qobjectdefs.h`, `qobject.h`, `qpointer.h`, `qcoreapplication.h`, `qtimer.h`

**Allowed:**

- `QMetaObject::invokeMethod(QObject*, const char* member, Qt::ConnectionType)` (char* overload; 0-arg slots)
- `Qt::QueuedConnection`, `Qt::BlockingQueuedConnection`
- `QObject::thread()`
- `QPointer<T>` with `data()` / `isNull()` / `clear()` — **not** `get()`
- `QCoreApplication::removePostedEvents(QObject* receiver, int eventType = 0)`
- `QTimer::start()`, `stop()`, single-shot debounce
- `QEvent::MetaCall` as the event type for `removePostedEvents`

**Forbidden:**

- `QPointer::get()` (Qt 6)
- `invokeMethod(obj, &Class::slot, type, arg1, …)` with arguments (Qt 6)

### Win32 hook

Sources: Microsoft Learn `HOOKPROC`, `LowLevelKeyboardProc`, `SetWindowsHookExW`. Copy-ready local reference: `OpenRGBRipplePlugin/sdk-client/OpenRGBRippleClient.cpp:441-457`.

```cpp
typedef LRESULT (CALLBACK *HOOKPROC)(int code, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
HHOOK SetWindowsHookExW(int idHook, HOOKPROC lpfn, HINSTANCE hmod, DWORD dwThreadId);
LRESULT CallNextHookEx(HHOOK hhk, int nCode, WPARAM wParam, LPARAM lParam);
BOOL UnhookWindowsHookEx(HHOOK hhk);
```

`WH_KEYBOARD_LL` is 13. Return `CallNextHookEx` **untruncated**. No `reinterpret_cast<HOOKPROC>`.

### RGBController methods we may call (already used)

- `SetCustomMode()` — mode index only; safe during rebuild
- `UpdateMode()` — queues USB on `DeviceCallThread`; **only** when session is live and DetectionStart has not begun
- `UpdateLEDs()` — same rule
- `type`, `name`, `zones[]`, `leds[]`, `colors[]`

Do not call `UpdateMode`/`UpdateLEDs` from `DetectionStart` or while `session.IsLive() == false`.

---

## File map (after the work)

| File | Responsibility |
| --- | --- |
| `OpenRGBRipplePlugin/DeviceSession.h` | Generation, pointer cache, rebuild, invalidate. No Qt widgets. |
| `OpenRGBRipplePlugin/DeviceSession.cpp` | Mapping walk (copy from `RippleWidget.cpp:537-613`) |
| `OpenRGBRipplePlugin/RippleSettingsIO.h` | Non-throwing JSON ↔ `RippleSettings` |
| `OpenRGBRipplePlugin/OpenRGBRipplePlugin.h` | Callbacks as 0-arg slots; `alive_`; owns `DeviceSession` |
| `OpenRGBRipplePlugin/OpenRGBRipplePlugin.cpp` | Register `this`; no sleep; copy controller snapshot in list-change |
| `OpenRGBRipplePlugin/RippleWidget.h/.cpp` | UI + paint + hook + debounce save. Calls session. No RM callbacks. |
| `OpenRGBRipplePlugin/KeyboardHook.h/.cpp` | Real `HOOKPROC` |
| `OpenRGBRipplePlugin/tests/hostless/test_ripple_engine.cpp` | Engine + `NameMatches` + settings IO |
| `OpenRGBRipplePlugin/CMakeLists.txt` + `.pro` | Sources + version `1.0.2` |

`RippleEngine.h` and `KeyMap.*` stay as-is except tests include them.

---

## Phase 1 — Task 0: Orchestrator creates worktrees

**Files:**

- Modify: `.gitignore`
- Create: `.worktrees/` (ignored)

- [ ] **Step 1: Ignore worktrees**

Append to `.gitignore`:

```
# Isolated implementation worktrees
.worktrees/
```

```bash
git add .gitignore
git commit -m "chore: ignore .worktrees for isolated hardening tracks"
```

If `.gitignore` is already dirty with unrelated edits, commit **only** the `.worktrees/` line.

- [ ] **Step 2: Cut the integration branch and three worktrees**

```bash
git checkout -b fix/publish-hardening
mkdir -p .worktrees
git worktree add .worktrees/fix-device-session -b fix/device-session
git worktree add .worktrees/fix-keyboard-hook -b fix/keyboard-hook
git worktree add .worktrees/fix-version-and-tests -b fix/version-and-tests
```

- [ ] **Step 3: Spawn three subagents in one turn**

Each prompt must include: this plan's Phase 0 Allowed APIs, the track's tasks below, anti-pattern guards, and "commit on your branch; do not merge; do not touch files you do not own."

- Track A cwd: `<repo>/.worktrees/fix-device-session`
- Track B cwd: `<repo>/.worktrees/fix-keyboard-hook`
- Track C cwd: `<repo>/.worktrees/fix-version-and-tests`

Use `subagent_type: general-purpose`. Do **not** pass `capability_mode: read-only`. Prefer `isolation: "none"` with `cwd` set to the worktree so commits land on the track branch.

---

## Phase 1 — Track A: DeviceSession + plugin-owned callbacks

**Worktree:** `.worktrees/fix-device-session`  
**Branch:** `fix/device-session`  
**Goal:** Delete the rescan/unload crash class.

### Task A1: Add `DeviceSession`

**Files:**

- Create: `OpenRGBRipplePlugin/DeviceSession.h`
- Create: `OpenRGBRipplePlugin/DeviceSession.cpp`
- Modify: `OpenRGBRipplePlugin/CMakeLists.txt` (add the two files to `add_library`)
- Modify: `OpenRGBRipplePlugin/OpenRGBRipplePlugin.pro` (add to HEADERS + SOURCES)

- [ ] **Step 1: Write `DeviceSession.h` exactly as follows**

Copy the mapping rules from `RippleWidget.cpp:551-610` (KEYBOARD/KEYPAD/LAPTOP, matrix `0xFFFFFFFF` skip, linear/single zones). Do **not** put `QCheckBox*` in this type.

```cpp
/*---------------------------------------------------------*\
| DeviceSession.h                                           |
|   Cached RGBController* + generation. No Qt widgets.      |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class RGBController;

class DeviceSession
{
public:
    struct MappedLed
    {
        RGBController* controller = nullptr;
        unsigned int   led_index  = 0;
        float          x          = 0;
        float          y          = 0;
    };

    struct DeviceOpt
    {
        RGBController* controller = nullptr;
        std::string    name;
        bool           selected   = true;
    };

    /* Detection-thread safe. Bumps generation, drops every pointer. */
    void Invalidate();

    uint64_t Generation() const { return generation_.load(); }
    bool     IsLive()     const { return live_.load(); }

    /* Walk a snapshot copied by the caller on the RM callback thread
       while GetRGBControllers() is stable. Does not call UpdateMode. */
    void Rebuild(const std::vector<RGBController*>& snapshot);

    void SetCustomModes();
    void PushDirectMode();

    bool DeviceSelected(RGBController* controller) const;
    void SetSelectedByName(const std::string& name, bool on);

    std::vector<DeviceOpt> Devices() const;
    std::vector<MappedLed> Mapped() const;

    template<typename Fn>
    bool WithLive(Fn&& fn)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!live_)
        {
            return false;
        }
        fn(devices_, mapped_);
        return true;
    }

private:
    void RebuildUnlocked(const std::vector<RGBController*>& snapshot);

    mutable std::mutex     mutex_;
    std::atomic<uint64_t>  generation_{0};
    std::atomic<bool>      live_{false};
    std::vector<DeviceOpt> devices_;
    std::vector<MappedLed> mapped_;
};
```

- [ ] **Step 2: Write `DeviceSession.cpp`**

Copy the controller filter and zone walk from `RippleWidget.cpp:551-610`. Critical rules:

- `Invalidate()`: lock, `live_ = false`, `generation_++`, `mapped_.clear()`, null every `DeviceOpt::controller` (keep `name` + `selected` so checkboxes can rebind after rescan).
- `Rebuild(snapshot)`: lock, `live_ = false`, clear `devices_`/`mapped_`, rebuild from snapshot, restore `selected` when `name` matches a previous row, `SetCustomMode()` only (copy `EnsureDirectMode` from `RippleWidget.cpp:468-478`), then `live_ = true`.
- `PushDirectMode()`: lock, if `!live_` return; else `UpdateMode()` on each non-null selected (or all, matching today's "none checked = all") controller.
- `DeviceSelected`: same policy as `RippleWidget.cpp:496-515` (`any` checked → only those; none checked → all).
- Never call `GetRGBControllers()` from this file.

Include `"RGBController.h"` and `"ResourceManagerInterface.h"` is **not** needed here — only `RGBController.h` for `type`, `name`, `zones`, `SetCustomMode`, `UpdateMode`, `DEVICE_TYPE_*`, `ZONE_TYPE_*`.

- [ ] **Step 3: Add sources to qmake and CMake**

`OpenRGBRipplePlugin.pro` HEADERS += `DeviceSession.h` `RippleSettingsIO.h`  
SOURCES += `DeviceSession.cpp`

`CMakeLists.txt` `add_library(...)` add `DeviceSession.cpp` `DeviceSession.h` `RippleSettingsIO.h`.

- [ ] **Step 4: Commit**

```bash
git add OpenRGBRipplePlugin/DeviceSession.h OpenRGBRipplePlugin/DeviceSession.cpp \
        OpenRGBRipplePlugin/CMakeLists.txt OpenRGBRipplePlugin/OpenRGBRipplePlugin.pro
git commit -m "Add DeviceSession to own RGBController pointers and generation."
```

### Task A2: Non-throwing settings helper

**Files:**

- Create: `OpenRGBRipplePlugin/RippleSettingsIO.h`

- [ ] **Step 1: Write the helper**

This file may include `"RippleEngine.h"` and OpenRGB's `"json.hpp"` / `"nlohmann/json.hpp"` — match whatever `RippleWidget.cpp` already includes via `SettingsManager.h` (`json` type). Wrap every `.get<T>()` in try/catch. On any exception or bad type, leave that field at its incoming default.

```cpp
#pragma once

#include "RippleEngine.h"

template<typename T>
static bool JsonGet(const json& j, const char* key, T& out)
{
    try
    {
        if(!j.contains(key))
        {
            return false;
        }
        out = j.at(key).get<T>();
        return true;
    }
    catch(...)
    {
        return false;
    }
}

static RippleSettings SettingsFromJson(const json& j, RippleSettings s)
{
    int brush = static_cast<int>(s.brush);
    int color_mode = static_cast<int>(s.color_mode);
    int blend = static_cast<int>(s.blend);
    JsonGet(j, "brush", brush);
    JsonGet(j, "speed", s.speed);
    JsonGet(j, "thickness", s.thickness);
    JsonGet(j, "lifetime", s.lifetime);
    JsonGet(j, "fade_power", s.fade_power);
    JsonGet(j, "echo_count", s.echo_count);
    JsonGet(j, "echo_delay", s.echo_delay);
    JsonGet(j, "brightness", s.brightness);
    JsonGet(j, "color_mode", color_mode);
    JsonGet(j, "impact_flash", s.impact_flash);
    JsonGet(j, "enabled", s.enabled);
    JsonGet(j, "solid_r", s.solid.r);
    JsonGet(j, "solid_g", s.solid.g);
    JsonGet(j, "solid_b", s.solid.b);
    JsonGet(j, "idle_r", s.idle.r);
    JsonGet(j, "idle_g", s.idle.g);
    JsonGet(j, "idle_b", s.idle.b);
    JsonGet(j, "blend", blend);
    JsonGet(j, "paint_idle", s.paint_idle);
    if(brush >= 0 && brush <= 2) s.brush = static_cast<RippleBrush>(brush);
    if(color_mode >= 0 && color_mode <= 2) s.color_mode = static_cast<RippleColorMode>(color_mode);
    if(blend >= 0 && blend <= 1) s.blend = static_cast<RippleBlend>(blend);
    return s;
}

static json SettingsToJson(const RippleSettings& s)
{
    json j;
    j["brush"] = static_cast<int>(s.brush);
    j["speed"] = s.speed;
    j["thickness"] = s.thickness;
    j["lifetime"] = s.lifetime;
    j["fade_power"] = s.fade_power;
    j["echo_count"] = s.echo_count;
    j["echo_delay"] = s.echo_delay;
    j["brightness"] = s.brightness;
    j["color_mode"] = static_cast<int>(s.color_mode);
    j["impact_flash"] = s.impact_flash;
    j["enabled"] = s.enabled;
    j["solid_r"] = s.solid.r;
    j["solid_g"] = s.solid.g;
    j["solid_b"] = s.solid.b;
    j["idle_r"] = s.idle.r;
    j["idle_g"] = s.idle.g;
    j["idle_b"] = s.idle.b;
    j["blend"] = static_cast<int>(s.blend);
    j["paint_idle"] = s.paint_idle;
    return j;
}
```

If `json` is not visible from this header alone, include the same OpenRGB json header `RippleWidget.cpp` gets via `SettingsManager.h`. Do not vendor a second JSON library.

- [ ] **Step 2: Commit**

```bash
git add OpenRGBRipplePlugin/RippleSettingsIO.h
git commit -m "Add non-throwing Ripple settings JSON helper."
```

### Task A3: Plugin owns callbacks; no sleep; snapshot copy

**Files:**

- Modify: `OpenRGBRipplePlugin/OpenRGBRipplePlugin.h`
- Modify: `OpenRGBRipplePlugin/OpenRGBRipplePlugin.cpp`

- [ ] **Step 1: Replace the plugin header private section**

```cpp
#include "DeviceSession.h"
#include <QPointer>
#include <atomic>

class RippleWidget;

class OpenRGBRipplePlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    /* existing Q_PLUGIN_METADATA / Q_INTERFACES / public overrides stay */

public slots:
    void OnDetectionStart();
    void OnDeviceListChanged();
    void OnDetectionEnd();

private:
    static void DetectionStartCallback(void* arg);
    static void DeviceListChangedCallback(void* arg);
    static void DetectionEndCallback(void* arg);

    void RegisterHostCallbacks();
    void UnregisterHostCallbacks();
    std::vector<RGBController*> CopyControllers() const;

    ResourceManagerInterface* rm_ = nullptr;
    DeviceSession             session_;
    QPointer<RippleWidget>    ui_;
    std::atomic<bool>         alive_{false};
    bool                      callbacks_registered_ = false;
};
```

`OnDetectionStart` / `OnDeviceListChanged` / `OnDetectionEnd` **must be 0-arg slots** so Qt 5.15 `invokeMethod(obj, "OnDetectionStart", type)` works.

- [ ] **Step 2: Rewrite callbacks and GetWidget/Unload**

Copy this control flow. Do not add `sleep_for`.

```cpp
void OpenRGBRipplePlugin::DetectionStartCallback(void* arg)
{
    auto* self = static_cast<OpenRGBRipplePlugin*>(arg);
    if(!self || !self->alive_.load())
    {
        return;
    }
    if(QThread::currentThread() == self->thread())
    {
        self->OnDetectionStart();
    }
    else
    {
        QMetaObject::invokeMethod(self, "OnDetectionStart",
                                  Qt::BlockingQueuedConnection);
    }
}

void OpenRGBRipplePlugin::DeviceListChangedCallback(void* arg)
{
    auto* self = static_cast<OpenRGBRipplePlugin*>(arg);
    if(!self || !self->alive_.load())
    {
        return;
    }
    if(QThread::currentThread() == self->thread())
    {
        self->OnDeviceListChanged();
    }
    else
    {
        /* Inside UpdateDeviceList the controller vector is stable.
           Block until GUI drops/rebuilds pointers before we return. */
        QMetaObject::invokeMethod(self, "OnDeviceListChanged",
                                  Qt::BlockingQueuedConnection);
    }
}

void OpenRGBRipplePlugin::DetectionEndCallback(void* arg)
{
    auto* self = static_cast<OpenRGBRipplePlugin*>(arg);
    if(!self || !self->alive_.load())
    {
        return;
    }
    QMetaObject::invokeMethod(self, "OnDetectionEnd", Qt::QueuedConnection);
}

void OpenRGBRipplePlugin::OnDetectionStart()
{
    if(!alive_.load())
    {
        return;
    }
    session_.Invalidate();
    if(RippleWidget* w = ui_.data())
    {
        w->PausePainting();
    }
}

void OpenRGBRipplePlugin::OnDeviceListChanged()
{
    if(!alive_.load() || !rm_)
    {
        return;
    }
    if(rm_->GetDetectionPercent() < 100)
    {
        session_.Invalidate();
        if(RippleWidget* w = ui_.data())
        {
            w->PausePainting();
        }
        return;
    }
    session_.Rebuild(CopyControllers());
    if(RippleWidget* w = ui_.data())
    {
        w->BindDevicesFromSession();
        session_.PushDirectMode();
        w->ResumePainting();
    }
}

void OpenRGBRipplePlugin::OnDetectionEnd()
{
    if(!alive_.load() || !rm_)
    {
        return;
    }
    session_.Rebuild(CopyControllers());
    if(RippleWidget* w = ui_.data())
    {
        w->BindDevicesFromSession();
        session_.PushDirectMode();
        w->ResumePainting();
    }
}

std::vector<RGBController*> OpenRGBRipplePlugin::CopyControllers() const
{
    std::vector<RGBController*>& live = rm_->GetRGBControllers();
    return std::vector<RGBController*>(live.begin(), live.end());
}
```

`GetWidget`:

```cpp
QWidget* OpenRGBRipplePlugin::GetWidget()
{
    if(!rm_)
    {
        return nullptr;
    }
    if(RippleWidget* existing = ui_.data())
    {
        return existing;
    }
    rm_->WaitForDeviceDetection();
    alive_ = true;
    RegisterHostCallbacks();          /* BEFORE any RGBController* is cached */
    auto* w = new RippleWidget(rm_, &session_);
    ui_ = w;
    session_.Rebuild(CopyControllers());
    w->BindDevicesFromSession();
    session_.PushDirectMode();
    w->StartRuntime();                /* hook + timer, after callbacks exist */
    return w;
}
```

`Unload`:

```cpp
void OpenRGBRipplePlugin::Unload()
{
    alive_ = false;
    UnregisterHostCallbacks();
    QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
    session_.Invalidate();
    if(RippleWidget* w = ui_.data())
    {
        w->Shutdown();
    }
    ui_.clear();
}
```

`RegisterHostCallbacks` registers **Start, DeviceListChange, End** with `this`. Does **not** register DetectionProgress. Guard with `callbacks_registered_` so a second `GetWidget` in one Load cycle does not double-register.

`Load()`: store `rm_`, `alive_ = false`. Do not create the widget.

`GetTrayMenu`: parent the menu to `ui_.data()` if non-null; connections already null-check.

Include `<QCoreApplication>`, `<QEvent>`, `<QThread>`, `"RippleWidget.h"`, `"RGBController.h"`. **Delete** `#include <chrono>` and `#include <thread>` and every `sleep_for`.

- [ ] **Step 3: Commit**

```bash
git add OpenRGBRipplePlugin/OpenRGBRipplePlugin.h OpenRGBRipplePlugin/OpenRGBRipplePlugin.cpp
git commit -m "Own ResourceManager callbacks on the plugin QObject."
```

### Task A4: Slim `RippleWidget` to UI + paint

**Files:**

- Modify: `OpenRGBRipplePlugin/RippleWidget.h`
- Modify: `OpenRGBRipplePlugin/RippleWidget.cpp`

- [ ] **Step 1: Change the public API**

Constructor becomes `RippleWidget(ResourceManagerInterface* rm, DeviceSession* session, QWidget* parent = nullptr)`.

Replace `RebuildDevices` / `InvalidateDevices` / `SuspendForDetection` / `ResumeAfterDetection` (those slots die — plugin no longer `invokeMethod`s them) with:

```cpp
void PausePainting();          /* timer->stop only */
void ResumePainting();         /* timer->start if session->IsLive() */
void BindDevicesFromSession(); /* rebuild checkbox list from session->Devices() */
void StartRuntime();           /* hook_->Start(); timer start — called from GetWidget */
void Shutdown();               /* keep */
```

Remove `devices_`, `mapped_`, `device_mutex_`, `devices_live_`, `EnsureDirectMode`, `PushDirectMode`, `RebuildDevices` from the widget. Keep a `DeviceSession* session_` (not owned).

Checkbox `toggled` writes `session_->SetSelectedByName(box->text().toStdString(), on)`.

- [ ] **Step 2: Ctor no longer starts the world**

```cpp
RippleWidget::RippleWidget(ResourceManagerInterface* rm, DeviceSession* session, QWidget* parent)
    : QWidget(parent), rm_(rm), session_(session)
{
    hook_ = new KeyboardHook();
    BuildUi();
    LoadSettings();
    /* Do NOT RebuildDevices / PushDirectMode / hook Start / timer start here. */
    timer_ = new QTimer(this);
    timer_->setInterval(16);
    connect(timer_, &QTimer::timeout, this, &RippleWidget::OnTick);
    save_timer_ = new QTimer(this);
    save_timer_->setSingleShot(true);
    save_timer_->setInterval(400);
    connect(save_timer_, &QTimer::timeout, this, &RippleWidget::FlushSettings);
}
```

`StartRuntime()`: `hook_->Start(); if(session_ && session_->IsLive()) timer_->start();`

`PausePainting()`: `if(timer_) timer_->stop();` — **do not** invalidate (plugin already did).

`ResumePainting()`: `if(timer_ && session_ && session_->IsLive()) timer_->start();`

`BindDevicesFromSession()`: delete old checkboxes (current `RippleWidget.cpp:526-535`), then for each `session_->Devices()` create a checkbox with `name` and `selected`. This runs on the GUI thread only.

- [ ] **Step 3: Paint / ConsumeKeys / OnTick use `session_->WithLive`**

```cpp
void RippleWidget::OnTick()
{
    if(!session_ || !session_->IsLive())
    {
        return;
    }
    ConsumeKeys();
    Paint();
    /* status from session_->Mapped().size() — copy under WithLive */
}

void RippleWidget::Paint()
{
    /* prune / enabled / passthrough unchanged */
    if(!session_)
    {
        return;
    }
    session_->WithLive([&](const auto& devices, const auto& mapped)
    {
        /* existing idle fill + SampleOver + UpdateLEDs body from
           RippleWidget.cpp:712-750, using DeviceSelected via session_ */
    });
}
```

`DeviceSelected` calls become `session_->DeviceSelected(controller)`.

- [ ] **Step 4: LoadSettings uses the helper; SaveSettings is debounced**

```cpp
void RippleWidget::LoadSettings()
{
    if(!rm_ || !rm_->GetSettingsManager())
    {
        return;
    }
    try
    {
        json j = rm_->GetSettingsManager()->GetSettings("OpenRGBRipplePlugin");
        engine_.SetSettings(SettingsFromJson(j, engine_.GetSettings()));
    }
    catch(...)
    {
        /* keep engine defaults */
    }
    SyncUiFromSettings(engine_.GetSettings());
}

void RippleWidget::SaveSettings()
{
    if(save_timer_)
    {
        save_timer_->start(); /* restart 400 ms */
    }
}

void RippleWidget::FlushSettings()
{
    if(!rm_ || !rm_->GetSettingsManager())
    {
        return;
    }
    try
    {
        json j = SettingsToJson(engine_.GetSettings());
        rm_->GetSettingsManager()->SetSettings("OpenRGBRipplePlugin", j);
        rm_->GetSettingsManager()->SaveSettings();
    }
    catch(...)
    {
    }
}
```

Destructor: `Shutdown(); FlushSettings();` (flush pending debounce). `OnUiChanged` still calls `SaveSettings()` (now debounced).

Include `"DeviceSession.h"` and `"RippleSettingsIO.h"`.

- [ ] **Step 5: Commit**

```bash
git add OpenRGBRipplePlugin/RippleWidget.h OpenRGBRipplePlugin/RippleWidget.cpp
git commit -m "Keep RippleWidget off the ResourceManager callback path."
```

### Track A verification (agent must run before returning)

```bash
# In the worktree
rg -n "sleep_for|this_thread" OpenRGBRipplePlugin/OpenRGBRipplePlugin.cpp
# expect: no matches

rg -n "RegisterDetectionStartCallback|RegisterDeviceListChangeCallback|RegisterDetectionEndCallback|RegisterDetectionProgressCallback" OpenRGBRipplePlugin
# expect: Start/List/End registered with `this`; Progress NOT registered

rg -n "static_cast<RippleWidget\*>" OpenRGBRipplePlugin/OpenRGBRipplePlugin.cpp
# expect: no matches

rg -n "devices_live_|device_mutex_" OpenRGBRipplePlugin/RippleWidget.*
# expect: no matches (moved to DeviceSession)

rg -n "UpdateMode|UpdateLEDs" OpenRGBRipplePlugin
# expect: UpdateMode only in DeviceSession::PushDirectMode
#         UpdateLEDs only inside WithLive in RippleWidget::Paint
```

---

## Phase 1 — Track B: KeyboardHook ABI

**Worktree:** `.worktrees/fix-keyboard-hook`  
**Branch:** `fix/keyboard-hook`  
**Goal:** `LowLevelProc` is a real `HOOKPROC`. Copy the SDK client.

### Task B1: Fix the hook signature

**Files:**

- Modify: `OpenRGBRipplePlugin/KeyboardHook.h:42-45`
- Modify: `OpenRGBRipplePlugin/KeyboardHook.cpp:41-70` and `:86-87`

- [ ] **Step 1: Change the header declaration**

Under `#ifdef _WIN32`, include the real types. Keep the header self-contained the way the SDK client does:

```cpp
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
```

Replace the fake proc with:

```cpp
#ifdef _WIN32
    static KeyboardHook* instance_;
    static LRESULT CALLBACK LowLevelProc(int code, WPARAM wparam, LPARAM lparam);
    HHOOK hook_ = nullptr;
#endif
```

Delete `void* hook_`. `IsRunning()` can stay as `running_`.

- [ ] **Step 2: Change the .cpp to match the SDK client**

Copy the shape from `OpenRGBRipplePlugin/sdk-client/OpenRGBRippleClient.cpp:441-457`:

```cpp
LRESULT CALLBACK KeyboardHook::LowLevelProc(int code, WPARAM wparam, LPARAM lparam)
{
    if(instance_ && code >= 0)
    {
        const KBDLLHOOKSTRUCT* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lparam);
        if(info && (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN))
        {
            if((info->flags & LLKHF_INJECTED) == 0)
            {
                KeyEvent ev;
                ev.vk       = info->vkCode;
                ev.scan     = info->scanCode;
                ev.extended = (info->flags & LLKHF_EXTENDED) != 0;
                ev.down     = true;
                Callback cb;
                {
                    std::lock_guard<std::mutex> lock(instance_->mutex_);
                    instance_->queue_.push_back(ev);
                    cb = instance_->callback_;
                }
                if(cb)
                {
                    cb(ev);
                }
            }
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}
```

`Start()`:

```cpp
    instance_ = this;
    hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelProc, self, 0);
```

No `reinterpret_cast<HOOKPROC>`. If `hook_` is already set or `instance_` is a different live object, `Start()` returns `false` and does not overwrite `instance_`.

`Stop()`: `UnhookWindowsHookEx(hook_); hook_ = nullptr;` then if `instance_ == this` set `instance_ = nullptr`.

- [ ] **Step 3: Commit**

```bash
git add OpenRGBRipplePlugin/KeyboardHook.h OpenRGBRipplePlugin/KeyboardHook.cpp
git commit -m "Give KeyboardHook a real HOOKPROC signature."
```

### Track B verification

```bash
rg -n "unsigned long long wparam|long __stdcall LowLevelProc|reinterpret_cast<HOOKPROC>|static_cast<long>\(CallNextHookEx" OpenRGBRipplePlugin/KeyboardHook.*
# expect: no matches

rg -n "LRESULT CALLBACK LowLevelProc" OpenRGBRipplePlugin/KeyboardHook.h OpenRGBRipplePlugin/KeyboardHook.cpp
# expect: both files
```

---

## Phase 1 — Track C: Version 1.0.2 + host-less tests

**Worktree:** `.worktrees/fix-version-and-tests`  
**Branch:** `fix/version-and-tests`

### Task C1: One version string

**Files:**

- Modify: `OpenRGBRipplePlugin/OpenRGBRipplePlugin.cpp:21` → `"1.0.2"`
- Modify: `OpenRGBRipplePlugin/RippleWidget.cpp:134` → `"Ripple 1.0.2"`
- Modify: `OpenRGBRipplePlugin/OpenRGBRipplePlugin.pro:7` → `VERSION = 1.0.2`
- Modify: `OpenRGBRipplePlugin/CMakeLists.txt:43` → `VERSION_STRING="1.0.2"`
- Modify: `OpenRGBRipplePlugin/README.md` if it states a plugin version (keep API 4 / 1.0rc3 wording; only bump the plugin version if mentioned)

- [ ] **Step 1: Apply the four literals. Commit.**

```bash
git add OpenRGBRipplePlugin/OpenRGBRipplePlugin.cpp \
        OpenRGBRipplePlugin/RippleWidget.cpp \
        OpenRGBRipplePlugin/OpenRGBRipplePlugin.pro \
        OpenRGBRipplePlugin/CMakeLists.txt
git commit -m "Set plugin version to 1.0.2 everywhere."
```

### Task C2: Host-less tests (no OpenRGB, no Qt)

**Files:**

- Create: `OpenRGBRipplePlugin/tests/hostless/test_ripple_engine.cpp`
- Create: `OpenRGBRipplePlugin/tests/CMakeLists.txt`

These tests must compile on Linux with only the standard library. They **must not** include `DeviceSession`, `RGBController`, or Qt.

- [ ] **Step 1: Write the test file**

```cpp
#include "../RippleEngine.h"
#include "../KeyMap.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

static int g_fails = 0;

static void Expect(bool ok, const char* msg)
{
    if(!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        g_fails++;
    }
}

int main()
{
    RippleEngine e;
    RippleSettings s = e.GetSettings();
    s.enabled = true;
    s.color_mode = RippleColorMode::Solid;
    s.solid = {255, 0, 0};
    s.idle = {0, 0, 0};
    s.brush = RippleBrush::Fill;
    s.speed = 10;
    s.lifetime = 1;
    s.thickness = 1;
    s.echo_count = 0;
    s.paint_idle = true;
    e.SetSettings(s);

    e.Spawn(0, 0, 0.0, 1);
    Expect(e.ActiveCount() == 1, "one ripple after spawn");
    RippleRGB c = e.Sample(0, 0, 0.01);
    Expect(c.r > 10, "origin is not idle immediately after spawn");

    e.Prune(10.0);
    Expect(e.ActiveCount() == 0, "prune drops expired ripples");

    Expect(KeyMap::NameMatches("Key: A", {"Key: A"}), "exact match");
    Expect(KeyMap::NameMatches("key: a", {"Key: A"}), "case fold");
    Expect(!KeyMap::NameMatches("Key: B", {"Key: A"}), "different key");
    Expect(KeyMap::NameMatches("Something Key: A", {"Key: A"}), "suffix match");

    if(g_fails)
    {
        std::fprintf(stderr, "%d failed\n", g_fails);
        return 1;
    }
    std::printf("hostless tests ok\n");
    return 0;
}
```

`KeyMap.cpp` compiles on Linux (`Aliases()` empty). `NameMatches` is portable.

- [ ] **Step 2: Standalone CMake for tests only**

`OpenRGBRipplePlugin/tests/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(OpenRGBRippleHostlessTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(test_ripple_engine
    hostless/test_ripple_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../KeyMap.cpp
)
target_include_directories(test_ripple_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_custom_target(check COMMAND test_ripple_engine DEPENDS test_ripple_engine)
```

Do **not** fold this into the plugin `CMakeLists.txt` (avoids fighting Track A on that file). Orchestrator documents both.

- [ ] **Step 3: Run the tests**

```bash
cmake -S OpenRGBRipplePlugin/tests -B OpenRGBRipplePlugin/tests/build
cmake --build OpenRGBRipplePlugin/tests/build
OpenRGBRipplePlugin/tests/build/test_ripple_engine
```

Expected stdout: `hostless tests ok` exit 0.

Add `OpenRGBRipplePlugin/tests/build/` to `.gitignore`.

- [ ] **Step 4: Commit**

```bash
git add OpenRGBRipplePlugin/tests/hostless/test_ripple_engine.cpp \
        OpenRGBRipplePlugin/tests/CMakeLists.txt \
        .gitignore
git commit -m "Add host-less RippleEngine and KeyMap tests."
```

---

## Phase 2 — Orchestrator merge

- [ ] **Step 1: Merge B, then C, then A onto `fix/publish-hardening`**

```bash
git checkout fix/publish-hardening
git merge --no-ff fix/keyboard-hook -m "Merge keyboard hook ABI fix."
git merge --no-ff fix/version-and-tests -m "Merge 1.0.2 version and host-less tests."
git merge --no-ff fix/device-session -m "Merge DeviceSession and plugin-owned callbacks."
```

If `CMakeLists.txt` / `.pro` conflict: keep **all** Track A sources **and** Track C `VERSION_STRING="1.0.2"`. Do not drop `DeviceSession.cpp`.

If `RippleWidget.cpp` title conflicts: final string is `"Ripple 1.0.2"`.

If `OpenRGBRipplePlugin.cpp` Version conflicts: `"1.0.2"`.

- [ ] **Step 2: Sanity compile of host-less tests after merge**

```bash
cmake -S OpenRGBRipplePlugin/tests -B OpenRGBRipplePlugin/tests/build
cmake --build OpenRGBRipplePlugin/tests/build
OpenRGBRipplePlugin/tests/build/test_ripple_engine
```

- [ ] **Step 3: Spawn one reviewer subagent (read-only) on the merge**

Prompt: re-read this plan's Phase 0 + Phase 3 grep list; report any remaining `sleep_for`, widget-as-callback-arg, fake `HOOKPROC`, uncaught `.get<`, `DetectionProgress` registration, `GetWidget` without idempotence. Do not edit.

---

## Phase 3 — Verification (final)

Run from `fix/publish-hardening`.

### Anti-pattern grep (must be clean)

```bash
rg -n "sleep_for|this_thread" OpenRGBRipplePlugin --glob '!sdk-client/**'
# no matches in the plugin (sdk-client 16 ms loop may remain)

rg -n "static_cast<RippleWidget\*>" OpenRGBRipplePlugin
# no matches

rg -n "RegisterDetectionProgressCallback" OpenRGBRipplePlugin
# no matches

rg -n "reinterpret_cast<HOOKPROC>|long __stdcall LowLevelProc|unsigned long long wparam" OpenRGBRipplePlugin
# no matches

rg -n "QPointer::get\(|invokeMethod\([^,]+, &" OpenRGBRipplePlugin
# no matches (Qt 6 APIs)

rg -n "ResourceManager\.h|DetectDeviceMutex|RegisterClientInfoChangeCallback" OpenRGBRipplePlugin
# no matches

rg -n "OpenRGBPluginAPIInterface|OnSDKCommand" OpenRGBRipplePlugin
# no matches (API 5)

rg -n "1\.0\.0" OpenRGBRipplePlugin --glob '!sdk-client/**'
# no plugin version 1.0.0 left (ignore npm / protocol ids)

rg -n 'info\.Version|QLabel\("Ripple|VERSION_STRING|VERSION =' OpenRGBRipplePlugin
# all 1.0.2
```

### Structural checks

```bash
rg -n "class DeviceSession" OpenRGBRipplePlugin/DeviceSession.h
rg -n "alive_" OpenRGBRipplePlugin/OpenRGBRipplePlugin.h
rg -n "removePostedEvents" OpenRGBRipplePlugin/OpenRGBRipplePlugin.cpp
rg -n "SettingsFromJson|FlushSettings" OpenRGBRipplePlugin
rg -n "StartRuntime|PausePainting|BindDevicesFromSession" OpenRGBRipplePlugin
```

### Tests

```bash
cmake -S OpenRGBRipplePlugin/tests -B OpenRGBRipplePlugin/tests/build
cmake --build OpenRGBRipplePlugin/tests/build
OpenRGBRipplePlugin/tests/build/test_ripple_engine
```

Expected: `hostless tests ok`.

### Manual (Windows + OpenRGB 1.0rc3) — orchestrator cannot run here; list for the human

1. Build DLL: `powershell -ExecutionPolicy Bypass -File .\build-plugin.ps1`
2. Enable plugin. Ripple tab appears. Type: keys wave.
3. **Rescan Devices** with Ripple enabled — OpenRGB must not crash; status shows pause then remaps.
4. Rescan twice in a row.
5. Connect an SDK client (or enable SDK server) — ripple must resume painting after list change, not stay dead.
6. Disable plugin in Settings → Plugins, re-enable — one widget, one hook, no crash.
7. Drag sliders — settings file is not rewritten every 16 ms (watch `%APPDATA%\OpenRGB\OpenRGB.json` mtime).
8. Corrupt the `OpenRGBRipplePlugin` object in that JSON to `"brush": "nope"` — plugin tab still opens on next launch.

### What this Linux agent cannot verify

Full plugin DLL (needs Qt 5.15 + MSVC + OpenRGB headers). State that explicitly in the merge summary. Do not claim the rescan path is runtime-tested unless the Windows steps above were run.

---

## Anti-pattern guards (paste into every subagent prompt)

- Do not invent ResourceManager lock APIs.
- Do not include `ResourceManager.h`.
- Do not use Plugin API 5 types.
- Do not `sleep_for` in a ResourceManager callback.
- Do not register `RippleWidget*` as a callback `void*`.
- Do not register `DetectionProgress`.
- Do not call `UpdateMode`/`UpdateLEDs` unless `DeviceSession::IsLive()`.
- Do not call `UpdateMode` from `Rebuild` / `SetCustomModes`.
- Do not put `QCheckBox*` in `DeviceSession`.
- Do not use `QPointer::get()` or Qt 6 `invokeMethod` with extra args.
- Do not `reinterpret_cast<HOOKPROC>`.
- Do not let `LoadSettings` / `SettingsFromJson` throw.
- Do not commit `public/OpenRGBRipplePlugin.zip` or `public/sdk-client/`.
- Do not start the paint timer in the `RippleWidget` constructor.
- Do not implement Tracks A/B/C in the same worktree.

---

## Spec coverage (self-review)

| Review finding | Task |
| --- | --- |
| Queued Resume after Unload UAF | A3 `alive_` + `removePostedEvents` + `QPointer` + callbacks on plugin |
| DeviceListChanged only invalidates | A3 `OnDeviceListChanged` rebuilds when percent ≥ 100 |
| Progress callback = invalidate | A3 does not register progress |
| LoadSettings throws | A2 + A4 |
| HOOKPROC ABI | B1 |
| 80 ms sleep | A3 deletes it |
| Invalidate after stop-timer | A3 invalidates session first, then `PausePainting` |
| GetWidget not idempotent | A3 returns existing `ui_` |
| Timer starts before callbacks | A3 register, then widget, then `StartRuntime` |
| GetRGBControllers without snapshot | A3 `CopyControllers()` on the callback path |
| Version 1.0.0 vs 1.0.1 | C1 → 1.0.2 |
| SaveSettings every slider tick | A4 400 ms debounce |
| God-object widget | A1 + A4 DeviceSession extract |
| No tests | C2 |

No remaining review blocker is unassigned.
