/*---------------------------------------------------------*\
| DeviceSession.cpp                                         |
|   Cached RGBController* + generation. No Qt widgets.      |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "DeviceSession.h"
#include "RGBController.h"

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

void DeviceSession::Rebuild(const std::vector<RGBController*>& snapshot)
{
    std::lock_guard<std::mutex> lock(mutex_);
    live_ = false;
    RebuildUnlocked(snapshot);
    live_ = true;
}

void DeviceSession::RebuildUnlocked(const std::vector<RGBController*>& snapshot)
{
    const std::vector<DeviceOpt> previous = devices_;
    devices_.clear();
    mapped_.clear();

    for(RGBController* controller : snapshot)
    {
        if(!controller)
        {
            continue;
        }
        const bool is_kb =
            controller->type == DEVICE_TYPE_KEYBOARD ||
            controller->type == DEVICE_TYPE_KEYPAD ||
            controller->type == DEVICE_TYPE_LAPTOP;
        if(!is_kb)
        {
            continue;
        }

        DeviceOpt opt;
        opt.controller = controller;
        opt.name       = controller->name;
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

        for(size_t z = 0; z < controller->zones.size(); z++)
        {
            zone& zn = controller->zones[z];
            if(zn.type == ZONE_TYPE_MATRIX && zn.matrix_map && zn.matrix_map->map)
            {
                const unsigned int h = zn.matrix_map->height;
                const unsigned int w = zn.matrix_map->width;
                for(unsigned int y = 0; y < h; y++)
                {
                    for(unsigned int x = 0; x < w; x++)
                    {
                        const unsigned int idx = zn.matrix_map->map[y * w + x];
                        if(idx == 0xFFFFFFFFu)
                        {
                            continue;
                        }
                        MappedLed led;
                        led.controller = controller;
                        led.led_index  = zn.start_idx + idx;
                        led.x          = static_cast<float>(x);
                        led.y          = static_cast<float>(y);
                        mapped_.push_back(led);
                    }
                }
            }
            else if(zn.type == ZONE_TYPE_LINEAR || zn.type == ZONE_TYPE_SINGLE)
            {
                for(unsigned int i = 0; i < zn.leds_count; i++)
                {
                    MappedLed led;
                    led.controller = controller;
                    led.led_index  = zn.start_idx + i;
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
            d.controller->UpdateMode();
        }
    }
}

bool DeviceSession::DeviceSelected(RGBController* controller) const
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
