/*---------------------------------------------------------*\
| DeviceSession.cpp                                         |
|   Cached controller ptrs + generation. No Qt widgets.     |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "DeviceSession.h"
#include "RGBControllerInterface.h"

void DeviceSession::Invalidate()
{
    std::lock_guard<std::mutex> lock(mutex_);
    live_ = false;
    generation_++;
    mapped_.clear();
    for(DeviceOpt& d : devices_)
    {
        d.controller = nullptr;
    }
}

void DeviceSession::Rebuild(const std::vector<RGBControllerInterface*>& snapshot)
{
    std::lock_guard<std::mutex> lock(mutex_);
    live_ = false;
    RebuildUnlocked(snapshot);
    live_ = true;
}

void DeviceSession::RebuildUnlocked(const std::vector<RGBControllerInterface*>& snapshot)
{
    const std::vector<DeviceOpt> previous = devices_;
    devices_.clear();
    mapped_.clear();

    for(RGBControllerInterface* controller : snapshot)
    {
        if(!controller)
        {
            continue;
        }
        const device_type type = controller->GetDeviceType();
        const bool is_kb =
            type == DEVICE_TYPE_KEYBOARD ||
            type == DEVICE_TYPE_KEYPAD ||
            type == DEVICE_TYPE_LAPTOP;
        if(!is_kb)
        {
            continue;
        }

        DeviceOpt opt;
        opt.controller = controller;
        opt.name       = controller->GetName();
        opt.selected   = true;
        for(const DeviceOpt& prev : previous)
        {
            if(prev.name == opt.name)
            {
                opt.selected = prev.selected;
                break;
            }
        }
        devices_.push_back(opt);

        /* SetCustomMode only picks the Direct/Custom mode index.
           Do not call UpdateMode() here — that queues a USB write. */
        controller->SetCustomMode();

        const unsigned int zone_count = controller->GetZoneCount();
        for(unsigned int z = 0; z < zone_count; z++)
        {
            const zone_type    zt    = controller->GetZoneType(z);
            const unsigned int start = controller->GetZoneStartIndex(z);
            /* Copy, not GetZoneMatrixMapData(): that points into the
               controller and dies with a ResizeZone. */
            const matrix_map_type mm = controller->GetZoneMatrixMap(z);
            const bool is_matrix =
                zt == ZONE_TYPE_MATRIX ||
                zt == ZONE_TYPE_MATRIX_LOOP_X ||
                zt == ZONE_TYPE_MATRIX_LOOP_Y;
            if(is_matrix && mm.height > 0 && mm.width > 0
               && mm.map.size() >= static_cast<size_t>(mm.height) * mm.width)
            {
                const unsigned int h = mm.height;
                const unsigned int w = mm.width;
                for(unsigned int y = 0; y < h; y++)
                {
                    for(unsigned int x = 0; x < w; x++)
                    {
                        const unsigned int idx = mm.map[y * w + x];
                        if(idx == 0xFFFFFFFFu)
                        {
                            continue;
                        }
                        MappedLed led;
                        led.controller = controller;
                        led.led_index  = start + idx;
                        led.x          = static_cast<float>(x);
                        led.y          = static_cast<float>(y);
                        mapped_.push_back(led);
                    }
                }
            }
            else if(zt == ZONE_TYPE_LINEAR || zt == ZONE_TYPE_LINEAR_LOOP
                    || zt == ZONE_TYPE_SINGLE)
            {
                const unsigned int count = controller->GetZoneLEDsCount(z);
                for(unsigned int i = 0; i < count; i++)
                {
                    MappedLed led;
                    led.controller = controller;
                    led.led_index  = start + i;
                    led.x          = static_cast<float>(i);
                    led.y          = 0.0f;
                    mapped_.push_back(led);
                }
            }
        }
    }
}

void DeviceSession::SetCustomModes()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for(const DeviceOpt& d : devices_)
    {
        if(d.controller)
        {
            d.controller->SetCustomMode();
        }
    }
}

void DeviceSession::PushDirectMode()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(!live_)
    {
        return;
    }
    for(const DeviceOpt& d : devices_)
    {
        if(d.controller && DeviceSelected(d.controller))
        {
            /* API 5 hides DeviceUpdateMode(). UpdateMode() flags the
               controller's DeviceCallThread, which runs a pending mode
               before pending LEDs, so the first frame is not lost. */
            d.controller->UpdateMode();
        }
    }
}

bool DeviceSession::DeviceSelected(RGBControllerInterface* controller) const
{
    if(!controller)
    {
        return false;
    }
    bool any = false;
    for(const DeviceOpt& d : devices_)
    {
        if(d.selected)
        {
            any = true;
            if(d.controller == controller)
            {
                return true;
            }
        }
    }
    return !any;
}

void DeviceSession::SetSelectedByName(const std::string& name, bool on)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for(DeviceOpt& d : devices_)
    {
        if(d.name == name)
        {
            d.selected = on;
        }
    }
}

std::vector<DeviceSession::DeviceOpt> DeviceSession::Devices() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_;
}

std::vector<DeviceSession::MappedLed> DeviceSession::Mapped() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return mapped_;
}
