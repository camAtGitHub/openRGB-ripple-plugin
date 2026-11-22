#pragma once

#include "RippleEngine.h"
#include <nlohmann/json.hpp>

#include <algorithm>

using json = nlohmann::json;

/* Idle 0-60 min, lights off 0-120 min (0 = off). With enforce_order, lights
   off must fire after idle; skip it when Effects is missing, since the
   saved idle timeout does nothing then. */
static void ClampIdleTimeouts(RippleSettings& s, bool enforce_order)
{
    s.idle_timeout_min = std::clamp(s.idle_timeout_min, 0, 60);
    s.lights_off_min   = std::clamp(s.lights_off_min, 0, 120);
    if(enforce_order && s.idle_timeout_min > 0 && s.lights_off_min > 0
       && s.lights_off_min <= s.idle_timeout_min)
    {
        s.lights_off_min = s.idle_timeout_min + 1;
    }
}

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
    int shape = static_cast<int>(s.shape);
    int color_mode = static_cast<int>(s.color_mode);
    int blend = static_cast<int>(s.blend);
    JsonGet(j, "brush", brush);
    JsonGet(j, "shape", shape);
    JsonGet(j, "speed", s.speed);
    JsonGet(j, "thickness", s.thickness);
    JsonGet(j, "lifetime", s.lifetime);
    if(!JsonGet(j, "fade", s.fade))
    {
        /* Legacy: fade_power was a dimming exponent, not retract seconds. May need retune. */
        JsonGet(j, "fade_power", s.fade);
    }
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
    JsonGet(j, "axis_jitter", s.axis_jitter);
    JsonGet(j, "sweep_span", s.sweep_span);
    JsonGet(j, "trail_length", s.trail_length);
    JsonGet(j, "blast_size", s.blast_size);
    int blast_shape = static_cast<int>(s.blast_shape);
    JsonGet(j, "blast_shape", blast_shape);
    JsonGet(j, "idle_timeout_min", s.idle_timeout_min);
    JsonGet(j, "lights_off_min", s.lights_off_min);
    /* Effects presence is unknown at load; the widget enforces order once shown. */
    ClampIdleTimeouts(s, false);
    if(brush >= 0 && brush <= 2) s.brush = static_cast<RippleBrush>(brush);
    if(shape >= 0 && shape <= 4) s.shape = static_cast<RippleShape>(shape);
    if(color_mode >= 0 && color_mode <= 2) s.color_mode = static_cast<RippleColorMode>(color_mode);
    if(blend >= 0 && blend < RippleBlendCount) s.blend = static_cast<RippleBlend>(blend);
    if(blast_shape == 0 || blast_shape == 1)
    {
        s.blast_shape = static_cast<RippleBlastShape>(blast_shape);
    }
    return s;
}

static json SettingsToJson(const RippleSettings& s)
{
    json j;
    j["brush"] = static_cast<int>(s.brush);
    j["shape"] = static_cast<int>(s.shape);
    j["speed"] = s.speed;
    j["thickness"] = s.thickness;
    j["lifetime"] = s.lifetime;
    j["fade"] = s.fade;
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
    j["axis_jitter"] = s.axis_jitter;
    j["sweep_span"] = s.sweep_span;
    j["trail_length"] = s.trail_length;
    j["blast_size"] = s.blast_size;
    j["blast_shape"] = static_cast<int>(s.blast_shape);
    j["idle_timeout_min"] = s.idle_timeout_min;
    j["lights_off_min"] = s.lights_off_min;
    return j;
}
