#include "../RippleEngine.h"
#include "../KeyMap.h"
#include "../DetectionLifecycle.h"
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

    /* DetectionStart bumps epoch and sets paused. That pause must not
       veto DetectionEnd — it is this cycle, not a newer Start. */
    Expect(DetectionEndIsCurrent(1, 1),
           "DetectionEnd after Start of the same epoch must resume");
    Expect(!DetectionEndIsCurrent(1, 2),
           "DetectionEnd captured before a newer Start is stale");
    Expect(DetectionEndIsCurrent(0, 0),
           "idle matching epochs are current");

    /* Over must not mix idle hue into a full-coverage wave. Fade dims
       the wave; it is not a hole that lets the background show through. */
    {
        RippleEngine over;
        RippleSettings os = over.GetSettings();
        os.enabled = true;
        os.color_mode = RippleColorMode::Solid;
        os.solid = {255, 0, 0};
        os.idle = {0, 0, 200};
        os.brush = RippleBrush::Fill;
        os.speed = 10;
        os.lifetime = 1;
        os.fade_power = 1;
        os.brightness = 1;
        os.echo_count = 0;
        os.impact_flash = false;
        os.blend = RippleBlend::Max;
        os.paint_idle = true;
        over.SetSettings(os);
        over.Spawn(0, 0, 0.0, 1);
        RippleRGB mid = over.Sample(0, 0, 0.5);
        Expect(mid.r > 80, "over mid-life still has red");
        Expect(mid.b < 40, "over does not mix idle blue into a full-coverage red wave");
        Expect(mid.g < 20, "over does not invent green");
    }

    {
        const RippleRGB src = {255, 0, 0};
        const RippleRGB dst = {0, 0, 200};
        RippleRGB screen = RippleEngine::BlendLayer(src, dst, 1.0f, RippleBlend::Screen);
        Expect(screen.r > 250 && screen.b > 190, "screen of red on blue keeps both");
        RippleRGB excl = RippleEngine::BlendLayer(src, dst, 1.0f, RippleBlend::Exclusion);
        Expect(excl.r > 250 && excl.b > 190, "exclusion of red on blue is red+blue");
        RippleRGB xored = RippleEngine::BlendLayer(src, dst, 1.0f, RippleBlend::Xor);
        Expect(xored.r > 250 && xored.b > 190, "xor of red on blue flips both channels");
        RippleRGB plus = RippleEngine::BlendLayer(src, dst, 1.0f, RippleBlend::Add);
        Expect(plus.r > 250 && plus.b > 190, "add/plus of red on blue stacks both");
        RippleRGB overlay = RippleEngine::BlendLayer(src, dst, 1.0f, RippleBlend::Overlay);
        Expect(overlay.r > 200, "overlay/hard-light of red on blue shows the wave red");
        Expect(overlay.b < 40, "overlay/hard-light of red on blue does not stay idle blue");
        RippleRGB dodge = RippleEngine::BlendLayer(src, dst, 1.0f, RippleBlend::ColorDodge);
        Expect(dodge.r > 200, "color dodge of red on blue shows red");
        RippleRGB faded = {168, 0, 0};
        RippleRGB dodge_fade = RippleEngine::BlendLayer(faded, dst, 1.0f, RippleBlend::ColorDodge);
        Expect(dodge_fade.r > 80, "color dodge still shows faded red on blue");

        /* Bitwise XOR of faded/off-by-one 128s becomes cream and matches Add.
           LED XOR must stay a complementary pink on orange/teal. */
        const RippleRGB orange = {255, 128, 0};
        const RippleRGB teal = {0, 128, 128};
        RippleRGB xor_ot = RippleEngine::BlendLayer(orange, teal, 1.0f, RippleBlend::Xor);
        Expect(xor_ot.r > 200 && xor_ot.g < 80 && xor_ot.b > 80,
               "xor orange/teal is hot pink, not cream");
        const RippleRGB faded_orange = {230, 115, 0};
        RippleRGB xor_fade = RippleEngine::BlendLayer(faded_orange, teal, 1.0f, RippleBlend::Xor);
        Expect(xor_fade.g < 80, "faded xor orange/teal stays pink, not cream");
        const RippleRGB orange2 = {255, 140, 20};
        const RippleRGB teal2 = {0, 120, 140};
        RippleRGB xor_off = RippleEngine::BlendLayer(orange2, teal2, 1.0f, RippleBlend::Xor);
        Expect(xor_off.g < 80, "xor still pink when channels are not exact 128");
    }

    if(g_fails)
    {
        std::fprintf(stderr, "%d failed\n", g_fails);
        return 1;
    }
    std::printf("hostless tests ok\n");
    return 0;
}
