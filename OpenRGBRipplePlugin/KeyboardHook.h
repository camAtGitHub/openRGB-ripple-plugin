/*---------------------------------------------------------*\
| KeyboardHook.h                                            |
|   Low-level key-down hook (Windows WH_KEYBOARD_LL)        |
|   Installed on the calling thread — OpenRGB's Qt loop     |
|   pumps messages, so no extra thread is required.         |
\*---------------------------------------------------------*/

#pragma once

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
    static long __stdcall LowLevelProc(int code, unsigned long long wparam, long long lparam);
#endif

    mutable std::mutex    mutex_;
    Callback              callback_;
    std::vector<KeyEvent> queue_;
    bool                  running_ = false;
#ifdef _WIN32
    void*                 hook_ = nullptr;
#endif
};
