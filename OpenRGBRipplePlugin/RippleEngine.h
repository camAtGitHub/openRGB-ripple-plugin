/*---------------------------------------------------------*\
| RippleEngine.h                                            |
|                                                           |
|   Artemis-style key-press ripple sampler                  |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>

struct RippleRGB
{
    float r = 0;
    float g = 0;
    float b = 0;
};

enum class RippleBrush : int
{
    Ring = 0,
    Fill = 1,
    Soft = 2,
};

enum class RippleColorMode : int
{
    Solid   = 0,
    Rainbow = 1,
    Random  = 2,
};

enum class RippleBlend : int
{
    Max = 0,
    Add = 1,
};

struct RippleSettings
{
    RippleBrush     brush        = RippleBrush::Ring;
    float           speed        = 14.0f;
    float           thickness    = 1.15f;
    float           lifetime     = 1.15f;
    float           fade_power   = 1.35f;
    int             echo_count   = 1;
    float           echo_delay   = 0.12f;
    float           brightness   = 1.0f;
    RippleRGB       idle         = {6.0f, 8.0f, 10.0f};
    RippleColorMode color_mode   = RippleColorMode::Rainbow;
    RippleRGB       solid        = {46.0f, 230.0f, 214.0f};
    bool            impact_flash = true;
    float           impact_hold  = 0.08f;
    RippleBlend     blend        = RippleBlend::Max;
    bool            enabled      = true;
    bool            paint_idle   = true; /* false = leave idle keys for Effects / shaders */
};

struct Ripple
{
    float     x     = 0;
    float     y     = 0;
    double    t0    = 0;
    RippleRGB color;
    int       echo  = 0;
};

class RippleEngine
{
public:
    void SetSettings(const RippleSettings& s)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_ = s;
    }

    RippleSettings GetSettings() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return settings_;
    }

    void Spawn(float x, float y, double now, uint32_t seed)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!settings_.enabled)
        {
            return;
        }

        RippleRGB color = ColorForPress(settings_, now, seed);
        Ripple base;
        base.x     = x;
        base.y     = y;
        base.t0    = now;
        base.color = color;
        base.echo  = 0;
        ripples_.push_back(base);

        for(int i = 1; i <= settings_.echo_count; i++)
        {
            Ripple echo = base;
            echo.t0   = now + static_cast<double>(i) * settings_.echo_delay;
            echo.echo = i;
            ripples_.push_back(echo);
        }
    }

    void Prune(double now)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const double life = settings_.lifetime
                          + settings_.echo_count * settings_.echo_delay;
        size_t w = 0;
        for(size_t i = 0; i < ripples_.size(); i++)
        {
            if(now - ripples_[i].t0 <= life)
            {
                ripples_[w++] = ripples_[i];
            }
        }
        ripples_.resize(w);
    }

    RippleRGB Sample(float x, float y, double now) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return SampleUnlocked(x, y, now, settings_.idle);
    }

    RippleRGB SampleOver(float x, float y, double now, RippleRGB base) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return SampleUnlocked(x, y, now, base);
    }

    size_t ActiveCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return ripples_.size();
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ripples_.clear();
    }

    static RippleRGB ColorForPress(const RippleSettings& s, double now, uint32_t seed)
    {
        if(s.color_mode == RippleColorMode::Solid)
        {
            return s.solid;
        }
        if(s.color_mode == RippleColorMode::Rainbow)
        {
            float h = static_cast<float>(std::fmod(now * 0.12 + seed * 0.17, 1.0));
            if(h < 0)
            {
                h += 1.0f;
            }
            return HsvToRgb(h, 0.82f, 1.0f);
        }
        float h = static_cast<float>(std::fmod(seed * 0.61803398875, 1.0));
        if(h < 0)
        {
            h += 1.0f;
        }
        return HsvToRgb(h, 0.78f, 1.0f);
    }

    static RippleRGB HsvToRgb(float h, float sat, float v)
    {
        const int i = static_cast<int>(std::floor(h * 6.0f));
        const float f = h * 6.0f - i;
        const float p = v * (1.0f - sat);
        const float q = v * (1.0f - f * sat);
        const float t = v * (1.0f - (1.0f - f) * sat);
        RippleRGB c;
        switch(i % 6)
        {
            case 0: c = {v, t, p}; break;
            case 1: c = {q, v, p}; break;
            case 2: c = {p, v, t}; break;
            case 3: c = {p, q, v}; break;
            case 4: c = {t, p, v}; break;
            default: c = {v, p, q}; break;
        }
        c.r *= 255.0f;
        c.g *= 255.0f;
        c.b *= 255.0f;
        return c;
    }

private:
    RippleRGB SampleUnlocked(float x, float y, double now, RippleRGB base) const
    {
        float r = base.r;
        float g = base.g;
        float b = base.b;

        for(const Ripple& ripple : ripples_)
        {
            const double elapsed = now - ripple.t0;
            if(elapsed < 0.0 || elapsed > settings_.lifetime)
            {
                continue;
            }

            const float radius = settings_.speed * static_cast<float>(elapsed);
            const float dx = x - ripple.x;
            const float dy = y - ripple.y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            const float fade = std::pow(
                std::max(0.0f, 1.0f - static_cast<float>(elapsed / settings_.lifetime)),
                settings_.fade_power);

            float intensity = 0.0f;
            if(settings_.brush == RippleBrush::Ring)
            {
                const float band = std::fabs(dist - radius);
                intensity = std::max(0.0f, 1.0f - band / std::max(0.05f, settings_.thickness));
            }
            else if(settings_.brush == RippleBrush::Fill)
            {
                if(dist <= radius)
                {
                    intensity = 1.0f - dist / std::max(radius, 0.001f);
                }
            }
            else
            {
                const float sigma = std::max(0.15f, settings_.thickness * 0.85f);
                const float d = dist - radius;
                intensity = std::exp(-(d * d) / (2.0f * sigma * sigma));
            }

            if(settings_.impact_flash && dist < 0.55f)
            {
                const float hold = settings_.impact_hold;
                const float flash = elapsed < hold
                    ? 1.0f
                    : std::max(0.0f, 1.0f - static_cast<float>((elapsed - hold) / (settings_.lifetime * 0.35f)));
                intensity = std::max(intensity, flash);
            }

            intensity *= fade * settings_.brightness;
            if(intensity <= 0.002f)
            {
                continue;
            }

            if(settings_.blend == RippleBlend::Add)
            {
                r = std::min(255.0f, r + ripple.color.r * intensity);
                g = std::min(255.0f, g + ripple.color.g * intensity);
                b = std::min(255.0f, b + ripple.color.b * intensity);
            }
            else
            {
                /* Over: ripple is a higher layer. Per-channel max hid red on yellow
                   because idle already held R=255 and G never decreased. */
                const float keep = 1.0f - intensity;
                r = ripple.color.r * intensity + r * keep;
                g = ripple.color.g * intensity + g * keep;
                b = ripple.color.b * intensity + b * keep;
            }
        }

        return {r, g, b};
    }

    mutable std::mutex     mutex_;
    RippleSettings         settings_;
    std::vector<Ripple>    ripples_;
};
