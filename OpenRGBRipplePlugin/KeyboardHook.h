/*---------------------------------------------------------*\
| KeyboardHook.h                                            |
|   Low-level key-down hook (Windows WH_KEYBOARD_LL)        |
|   Installed on the calling thread — OpenRGB's Qt loop     |
|   pumps messages, so no extra thread is required.         |
\*---------------------------------------------------------*/

#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

struct KeyEvent
{
    uint32_t vk       = 0;
    uint32_t scan     = 0;
    bool     extended = false;
    bool     down     = true;
};

class KeyboardHook
{
public:
    using Callback = std::function<void(const KeyEvent&)>;

    KeyboardHook();
    ~KeyboardHook();

    KeyboardHook(const KeyboardHook&) = delete;
    KeyboardHook& operator=(const KeyboardHook&) = delete;

    bool Start();
    void Stop();
    bool IsRunning() const { return running_; }

    void SetCallback(Callback cb);
    std::vector<KeyEvent> Drain();

private:
#ifdef _WIN32
    static KeyboardHook* instance_;
    static LRESULT CALLBACK LowLevelProc(int code, WPARAM wparam, LPARAM lparam);
    HHOOK hook_ = nullptr;
#endif

    mutable std::mutex    mutex_;
    Callback              callback_;
    std::vector<KeyEvent> queue_;
    bool                  running_ = false;
};
