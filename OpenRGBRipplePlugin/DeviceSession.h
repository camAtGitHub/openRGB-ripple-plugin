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
