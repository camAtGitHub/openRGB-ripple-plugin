/*---------------------------------------------------------*\
| KeyboardHook.cpp                                          |
\*---------------------------------------------------------*/

#include "KeyboardHook.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef _WIN32
KeyboardHook* KeyboardHook::instance_ = nullptr;
#endif

KeyboardHook::KeyboardHook() = default;

KeyboardHook::~KeyboardHook()
{
    Stop();
}

void KeyboardHook::SetCallback(Callback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(cb);
}

std::vector<KeyEvent> KeyboardHook::Drain()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<KeyEvent> out;
    out.swap(queue_);
    return out;
}

#ifdef _WIN32

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

bool KeyboardHook::Start()
{
    if(hook_ || (instance_ && instance_ != this))
    {
        return false;
    }

    HMODULE self = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&KeyboardHook::LowLevelProc),
        &self);

    instance_ = this;
    hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelProc, self, 0);
    running_ = hook_ != nullptr;
    if(!running_)
    {
        if(instance_ == this)
        {
            instance_ = nullptr;
        }
    }
    return running_;
}

void KeyboardHook::Stop()
{
    if(hook_)
    {
        UnhookWindowsHookEx(hook_);
        hook_ = nullptr;
    }
    running_ = false;
    if(instance_ == this)
    {
        instance_ = nullptr;
    }
}

#else

bool KeyboardHook::Start()
{
    running_ = false;
    return false;
}

void KeyboardHook::Stop()
{
    running_ = false;
}

#endif
