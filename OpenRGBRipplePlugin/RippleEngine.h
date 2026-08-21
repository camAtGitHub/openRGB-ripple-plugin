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
    Max        = 0, /* Over: wave replaces idle where the brush covers */
    Add        = 1, /* Plus */
    Xor        = 2,
    Screen     = 3,
    Overlay    = 4,
    ColorDodge = 5,
    ColorBurn  = 6,
    Exclusion  = 7,
};

static constexpr int RippleBlendCount = 8;

struct RippleSettings
{
    RippleBrush     brush        = RippleBrush::Ring;
    float           speed        = 14.0f;
    float           thickness    = 1.15f;
    float           lifetime     = 1.15f;
    float           fade         = 1.0f; /* seconds to retract; 0 = snap off */
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
                          + settings_.fade
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

    /* Expand for `expand` seconds, then retract to origin over `fade` seconds.
       fade <= 1e-4 → false (snap off). peak = maxRadius if directed else speed * expandT.
       After expand: u = (elapsed - expand) / fade; radius = peak * (1 - u). */
    static bool WaveRadius(float speed, float elapsed, float expand, float fade,
                           float& radius, float maxRadius = 0.0f)
    {
        if(elapsed < 0.0f)
        {
            return false;
        }
        const float expandT = std::max(expand, 1e-6f);
        const bool directed = maxRadius > 0.0f;
        const float peak = directed ? maxRadius : speed * expandT;
        const float outSpeed = peak / expandT;
        if(elapsed <= expand)
        {
            radius = outSpeed * elapsed;
            return true;
        }
        if(fade <= 1e-4f)
        {
            return false;
        }
        const float u = (elapsed - expand) / fade;
        if(u >= 1.0f)
        {
            return false;
        }
        radius = peak * (1.0f - u);
        return true;
    }

    /* Key-widths per full hue cycle. Origin is red; outward walks the spectrum. */
    static constexpr float RainbowKeysPerCycle = 8.0f;

    static RippleRGB RainbowAtDistance(float dist, float hue_offset = 0.0f)
    {
        float h = std::fmod(dist / RainbowKeysPerCycle + hue_offset, 1.0f);
        if(h < 0.0f)
        {
            h += 1.0f;
        }
        return HsvToRgb(h, 0.82f, 1.0f);
    }

    static RippleRGB ColorForPress(const RippleSettings& s, double now, uint32_t seed)
    {
        if(s.color_mode == RippleColorMode::Solid)
        {
            return s.solid;
        }
        if(s.color_mode == RippleColorMode::Rainbow)
        {
            /* Placeholder: SampleUnlocked replaces this with RainbowAtDistance. */
            return RainbowAtDistance(0.0f);
        }
        float h = static_cast<float>(std::fmod(seed * 0.61803398875, 1.0));
        if(h < 0)
        {
            h += 1.0f;
        }
        return HsvToRgb(h, 0.78f, 1.0f);
    }

    /* src is the faded wave colour. coverage is brush shape only (0–1).
       Fade must not be folded into coverage or Over becomes a mix with idle. */
    static RippleRGB BlendLayer(RippleRGB src, RippleRGB dst, float coverage, RippleBlend mode)
    {
        if(coverage <= 0.002f)
        {
            return dst;
        }
        if(coverage > 1.0f)
        {
            coverage = 1.0f;
        }

        const float sr = src.r / 255.0f;
        const float sg = src.g / 255.0f;
        const float sb = src.b / 255.0f;
        const float dr = dst.r / 255.0f;
        const float dg = dst.g / 255.0f;
        const float db = dst.b / 255.0f;

        float br = sr;
        float bg = sg;
        float bb = sb;
        switch(mode)
        {
            case RippleBlend::Add:
                br = std::min(1.0f, sr + dr);
                bg = std::min(1.0f, sg + dg);
                bb = std::min(1.0f, sb + db);
                break;
            case RippleBlend::Xor:
                /* |src-dst|, not bitwise XOR. Fade or a 128 that is
                   actually 120 makes 128^x explode to cream and match Add. */
                br = std::fabs(sr - dr);
                bg = std::fabs(sg - dg);
                bb = std::fabs(sb - db);
                break;
            case RippleBlend::Screen:
                br = 1.0f - (1.0f - sr) * (1.0f - dr);
                bg = 1.0f - (1.0f - sg) * (1.0f - dg);
                bb = 1.0f - (1.0f - sb) * (1.0f - db);
                break;
            case RippleBlend::Overlay:
                /* Hard Light: the wave is the top layer and decides
                   multiply vs screen. Photoshop Overlay uses the
                   background, which is a no-op on 0/255 LED primaries. */
                br = HardLightChannel(sr, dr);
                bg = HardLightChannel(sg, dg);
                bb = HardLightChannel(sb, db);
                break;
            case RippleBlend::ColorDodge:
                br = ColorDodgeChannel(sr, dr);
                bg = ColorDodgeChannel(sg, dg);
                bb = ColorDodgeChannel(sb, db);
                break;
            case RippleBlend::ColorBurn:
                br = ColorBurnChannel(sr, dr);
                bg = ColorBurnChannel(sg, dg);
                bb = ColorBurnChannel(sb, db);
                break;
            case RippleBlend::Exclusion:
                br = sr + dr - 2.0f * sr * dr;
                bg = sg + dg - 2.0f * sg * dg;
                bb = sb + db - 2.0f * sb * db;
                break;
            case RippleBlend::Max:
            default:
                br = sr;
                bg = sg;
                bb = sb;
                break;
        }

        const float keep = 1.0f - coverage;
        return {
            (br * coverage + dr * keep) * 255.0f,
            (bg * coverage + dg * keep) * 255.0f,
            (bb * coverage + db * keep) * 255.0f
        };
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
    static float HardLightChannel(float s, float d)
    {
        return s < 0.5f ? 2.0f * s * d : 1.0f - 2.0f * (1.0f - s) * (1.0f - d);
    }

    static float ColorDodgeChannel(float s, float d)
    {
        /* Classic dodge of 0 dest stays 0, so a red wave on blue is
           invisible. Empty dest channels take the wave instead. */
        if(d <= 0.0f)
        {
            return s;
        }
        if(s >= 1.0f)
        {
            return 1.0f;
        }
        const float v = d / (1.0f - s);
        return v > 1.0f ? 1.0f : v;
    }

    static float ColorBurnChannel(float s, float d)
    {
        if(s <= 0.0f)
        {
            return 0.0f;
        }
        const float v = 1.0f - (1.0f - d) / s;
        return v < 0.0f ? 0.0f : v;
    }

    RippleRGB SampleUnlocked(float x, float y, double now, RippleRGB base) const
    {
        float r = base.r;
        float g = base.g;
        float b = base.b;

        for(const Ripple& ripple : ripples_)
        {
            const double elapsed = now - ripple.t0;
            if(elapsed < 0.0)
            {
                continue;
            }

            const float expand = settings_.lifetime;
            float radius = 0.0f;
            if(!WaveRadius(settings_.speed, static_cast<float>(elapsed),
                           expand, settings_.fade, radius))
            {
                continue;
            }

            const float dx = x - ripple.x;
            const float dy = y - ripple.y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            float coverage = 0.0f;
            if(settings_.brush == RippleBrush::Ring)
            {
                /* Parabolic crest: a linear tent is ~0.6 at the next key
                   and mixes idle through. 1-t² stays opaque across the band. */
                const float band = std::fabs(dist - radius);
                const float t = band / std::max(0.05f, settings_.thickness);
                coverage = t >= 1.0f ? 0.0f : 1.0f - t * t;
            }
            else if(settings_.brush == RippleBrush::Fill)
            {
                /* Hard front. Fade retracts the radius; it is not a wash. */
                coverage = dist <= radius ? 1.0f : 0.0f;
            }
            else
            {
                const float sigma = std::max(0.15f, settings_.thickness * 0.85f);
                const float d = dist - radius;
                coverage = std::exp(-(d * d) / (2.0f * sigma * sigma));
            }

            if(settings_.impact_flash && dist < 0.55f)
            {
                const float hold = settings_.impact_hold;
                const float flash = elapsed < hold
                    ? 1.0f
                    : std::max(0.0f, 1.0f - static_cast<float>((elapsed - hold) / (expand * 0.35f)));
                coverage = std::max(coverage, flash);
            }

            if(coverage <= 0.002f)
            {
                continue;
            }

            /* Coverage is the front; RGB is wave colour * brightness only. */
            const RippleRGB wave = settings_.color_mode == RippleColorMode::Rainbow
                ? RainbowAtDistance(dist)
                : ripple.color;
            const RippleRGB src = {
                wave.r * settings_.brightness,
                wave.g * settings_.brightness,
                wave.b * settings_.brightness
            };
            const RippleRGB out = BlendLayer(src, {r, g, b}, coverage, settings_.blend);
            r = out.r;
            g = out.g;
            b = out.b;
        }

        return {r, g, b};
    }

    mutable std::mutex     mutex_;
    RippleSettings         settings_;
    std::vector<Ripple>    ripples_;
};
