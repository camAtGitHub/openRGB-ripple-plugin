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

    /* Over must not mix idle hue into a full-coverage wave. */
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
        os.fade = 1;
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

    /* Ring on a key grid rarely hits dist==radius. A linear tent at
       1 unit off a 2.6-thick ring is only ~0.6 coverage and looks washed
       next to Soft. The crest should stay mostly the wave colour. */
    {
        RippleEngine ring;
        RippleSettings rs = ring.GetSettings();
        rs.enabled = true;
        rs.color_mode = RippleColorMode::Solid;
        rs.solid = {255, 0, 0};
        rs.idle = {0, 0, 200};
        rs.brush = RippleBrush::Ring;
        rs.speed = 1;
        rs.lifetime = 10;
        rs.thickness = 2.6f;
        rs.fade = 10;
        rs.brightness = 1;
        rs.echo_count = 0;
        rs.impact_flash = false;
        rs.blend = RippleBlend::Max;
        rs.paint_idle = true;
        ring.SetSettings(rs);
        ring.Spawn(0, 0, 0.0, 1);
        RippleRGB off = ring.Sample(1, 0, 2.0);
        Expect(off.r > 180, "ring 1 unit off crest still mostly red");
        Expect(off.b < 50, "ring 1 unit off crest is not washed with idle");
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

    /* Phase 2: fade is retract seconds; fill is a hard front. */
    {
        RippleEngine snap;
        RippleSettings ss = snap.GetSettings();
        ss.enabled = true;
        ss.color_mode = RippleColorMode::Solid;
        ss.solid = {255, 0, 0};
        ss.idle = {0, 0, 0};
        ss.brush = RippleBrush::Fill;
        ss.speed = 10;
        ss.lifetime = 1;
        ss.fade = 0;
        ss.brightness = 1;
        ss.echo_count = 0;
        ss.impact_flash = false;
        ss.blend = RippleBlend::Max;
        ss.paint_idle = true;
        snap.SetSettings(ss);
        snap.Spawn(0, 0, 0.0, 1);
        RippleRGB after = snap.Sample(0, 0, 1.01);
        Expect(after.r < 1 && after.g < 1 && after.b < 1,
               "fade 0 snaps off after lifetime, no dim ghost");
    }

    {
        RippleEngine retract;
        RippleSettings rs = retract.GetSettings();
        rs.enabled = true;
        rs.color_mode = RippleColorMode::Solid;
        rs.solid = {255, 0, 0};
        rs.idle = {0, 0, 0};
        rs.brush = RippleBrush::Fill;
        rs.speed = 10;
        rs.lifetime = 1;
        rs.fade = 3;
        rs.brightness = 1;
        rs.echo_count = 0;
        rs.impact_flash = false;
        rs.blend = RippleBlend::Max;
        rs.paint_idle = true;
        retract.SetSettings(rs);
        retract.Spawn(0, 0, 0.0, 1);
        RippleRGB far = retract.Sample(9, 0, 2.5);
        RippleRGB near = retract.Sample(2, 0, 2.5);
        Expect(far.r < 1, "retract radius 5: dist 9 idle");
        Expect(near.r > 200, "retract radius 5: dist 2 on");
    }

    {
        RippleEngine hard;
        RippleSettings hs = hard.GetSettings();
        hs.enabled = true;
        hs.color_mode = RippleColorMode::Solid;
        hs.solid = {255, 0, 0};
        hs.idle = {0, 0, 0};
        hs.brush = RippleBrush::Fill;
        hs.speed = 10;
        hs.lifetime = 1;
        hs.fade = 1;
        hs.brightness = 1;
        hs.echo_count = 0;
        hs.impact_flash = false;
        hs.blend = RippleBlend::Max;
        hs.paint_idle = true;
        hard.SetSettings(hs);
        hard.Spawn(0, 0, 0.0, 1);
        RippleRGB edge = hard.Sample(4.95f, 0, 0.5);
        Expect(edge.r > 200, "fill at 0.99*radius is full wave colour, not wash");
    }

    if(g_fails)
    {
        std::fprintf(stderr, "%d failed\n", g_fails);
        return 1;
    }
    std::printf("hostless tests ok\n");
    return 0;
}
