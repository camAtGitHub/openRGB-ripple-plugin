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

    /* Phase 3: Chebyshev square vs Euclidean circle. fade 0, speed 10, t=0.5 radius 5. */
    {
        RippleEngine sq;
        RippleSettings ss = sq.GetSettings();
        ss.enabled = true;
        ss.color_mode = RippleColorMode::Solid;
        ss.solid = {255, 0, 0};
        ss.idle = {0, 0, 0};
        ss.brush = RippleBrush::Fill;
        ss.shape = RippleShape::Square;
        ss.speed = 10;
        ss.lifetime = 1;
        ss.fade = 0;
        ss.brightness = 1;
        ss.echo_count = 0;
        ss.impact_flash = false;
        ss.blend = RippleBlend::Max;
        ss.paint_idle = true;
        sq.SetSettings(ss);
        sq.Spawn(0, 0, 0.0, 1);
        RippleRGB inside = sq.Sample(4, 4, 0.5);
        Expect(inside.r > 200, "square fill Chebyshev (4,4) on");
        RippleRGB outside = sq.Sample(5.1f, 0, 0.5);
        Expect(outside.r < 1 && outside.g < 1 && outside.b < 1,
               "square fill (5.1,0) idle");
        RippleRGB corner = sq.Sample(5, 5, 0.5);
        Expect(corner.r > 200, "square fill Chebyshev corner (5,5) on");
    }

    {
        RippleEngine circ;
        RippleSettings cs = circ.GetSettings();
        cs.enabled = true;
        cs.color_mode = RippleColorMode::Solid;
        cs.solid = {255, 0, 0};
        cs.idle = {0, 0, 0};
        cs.brush = RippleBrush::Fill;
        cs.shape = RippleShape::Circle;
        cs.speed = 10;
        cs.lifetime = 1;
        cs.fade = 0;
        cs.brightness = 1;
        cs.echo_count = 0;
        cs.impact_flash = false;
        cs.blend = RippleBlend::Max;
        cs.paint_idle = true;
        circ.SetSettings(cs);
        circ.Spawn(0, 0, 0.0, 1);
        RippleRGB euclid_corner = circ.Sample(5, 5, 0.5);
        Expect(euclid_corner.r < 1 && euclid_corner.g < 1 && euclid_corner.b < 1,
               "circle fill Euclidean corner (5,5) idle");
        RippleRGB near = circ.Sample(4.95f, 0, 0.5);
        Expect(near.r > 200, "circle fill (4.95,0) on");
    }

    {
        float r = 0.0f;
        const bool ok = RippleEngine::WaveRadius(14.0f, 0.35f, 0.35f, 0.0f, r, 14.0f);
        Expect(ok && std::fabs(r - 14.0f) < 1e-4f,
               "directed waveRadius lifetime 0.35 travel 14 -> radius 14");
    }

    /* jitter 0 from np0: only left (h dir -1) or up (v dir -1). */
    {
        LayoutBounds b; /* 0, 22.6, 0, 6 */
        const float nx = 19.6f;
        const float ny = 5.5f;
        /* Studio pickAxisDirection np0 jitter 0, seeds 1..64: L=left U=up. */
        const char* studio =
            "LLUULULLLUULULLL"
            "ULLULLLULUUULLLL"
            "LLLULLULLLLULLUL"
            "ULLUULLLULULLLUU";
        for(uint32_t seed = 1; seed <= 64; seed++)
        {
            AxisHeading h = RippleEngine::PickAxisDirection(nx, ny, b, seed, 0.0f);
            const bool left = h.axis == RippleAxis::Horizontal && h.dir < 0;
            const bool up = h.axis == RippleAxis::Vertical && h.dir < 0;
            char msg[80];
            std::snprintf(msg, sizeof(msg), "np0 jitter 0 seed %u is left or up", seed);
            Expect(left || up, msg);
            const char want = studio[seed - 1];
            std::snprintf(msg, sizeof(msg), "np0 jitter 0 seed %u matches studio", seed);
            Expect((want == 'L' && left) || (want == 'U' && up), msg);
        }

        RippleEngine ax;
        RippleSettings as = ax.GetSettings();
        as.enabled = true;
        as.color_mode = RippleColorMode::Solid;
        as.solid = {255, 0, 0};
        as.idle = {0, 0, 0};
        as.brush = RippleBrush::Fill;
        as.shape = RippleShape::Axis;
        as.axis_jitter = 0.0f;
        as.speed = 10;
        as.lifetime = 1;
        as.fade = 0;
        as.brightness = 1;
        as.echo_count = 0;
        as.impact_flash = false;
        as.blend = RippleBlend::Max;
        as.paint_idle = true;
        ax.SetSettings(as);
        /* t=0.8 so up travel/2=2.75 would miss (ny-3); late expand lights long-way. */
        const double t = 0.8;
        for(uint32_t seed = 1; seed <= 64; seed++)
        {
            ax.Clear();
            ax.Spawn(nx, ny, 0.0, seed, b);
            RippleRGB right = ax.Sample(nx + 2.0f, ny, t);
            RippleRGB down  = ax.Sample(nx, ny + 0.6f, t);
            RippleRGB left  = ax.Sample(nx - 3.0f, ny, t);
            RippleRGB up    = ax.Sample(nx, ny - 3.0f, t);
            char rmsg[96];
            std::snprintf(rmsg, sizeof(rmsg),
                          "np0 jitter 0 seed %u short-way right idle", seed);
            Expect(right.r < 1 && right.g < 1 && right.b < 1, rmsg);
            char dmsg[96];
            std::snprintf(dmsg, sizeof(dmsg),
                          "np0 jitter 0 seed %u short-way down off-lane idle", seed);
            Expect(down.r < 1 && down.g < 1 && down.b < 1, dmsg);
            char lmsg[96];
            std::snprintf(lmsg, sizeof(lmsg),
                          "np0 jitter 0 seed %u long-way left or up on", seed);
            Expect(left.r > 200 || up.r > 200, lmsg);
        }
    }

    /* Phase 4: Dart + last-key blast. */
    {
        RippleEngine dart;
        RippleSettings ds = dart.GetSettings();
        ds.enabled = true;
        ds.color_mode = RippleColorMode::Solid;
        ds.solid = {255, 0, 0};
        ds.idle = {0, 0, 0};
        ds.brush = RippleBrush::Fill;
        ds.shape = RippleShape::Jump;
        ds.speed = 10;
        ds.lifetime = 1;
        ds.fade = 0;
        ds.brightness = 1;
        ds.echo_count = 0;
        ds.impact_flash = false;
        ds.blend = RippleBlend::Max;
        ds.paint_idle = true;
        ds.trail_length = 2.5f;
        dart.SetSettings(ds);
        dart.Spawn(0, 0, 0.0, 1); /* no from → same-key */
        RippleRGB far = dart.Sample(10, 0, 0.01);
        Expect(far.r < 1 && far.g < 1 && far.b < 1,
               "same-key dart does not light a point 10 units away");
        RippleRGB land = dart.Sample(0, 0, 0.01);
        Expect(land.r > 200, "same-key dart lights landing key");
        RippleRGB on = dart.Sample(0.55f, 0, 0.01);
        Expect(on.r > 200, "same-key dart d<=0.55 is lit");
    }

    {
        RippleEngine dart;
        RippleSettings ds = dart.GetSettings();
        ds.enabled = true;
        ds.color_mode = RippleColorMode::Solid;
        ds.solid = {255, 0, 0};
        ds.idle = {0, 0, 0};
        ds.brush = RippleBrush::Fill;
        ds.shape = RippleShape::Jump;
        ds.speed = 10;
        ds.lifetime = 1;
        ds.fade = 0;
        ds.brightness = 1;
        ds.echo_count = 0;
        ds.impact_flash = false;
        ds.blend = RippleBlend::Max;
        ds.paint_idle = true;
        ds.trail_length = 0.0f;
        dart.SetSettings(ds);
        dart.Spawn(10, 0, 0.0, 1, LayoutBounds{}, true, 0.0f, 0.0f);
        const double mid = 0.5; /* head at 5 */
        RippleRGB head = dart.Sample(5, 0, mid);
        Expect(head.r > 200, "two-point dart mid-flight: head on");
        RippleRGB ahead = dart.Sample(8, 0, mid);
        Expect(ahead.r < 1 && ahead.g < 1 && ahead.b < 1,
               "two-point dart mid-flight: ahead off");
        RippleRGB behind = dart.Sample(3, 0, mid);
        Expect(behind.r < 1 && behind.g < 1 && behind.b < 1,
               "trail_length=0 behind head off");
    }

    {
        RippleEngine dart;
        RippleSettings ds = dart.GetSettings();
        ds.enabled = true;
        ds.color_mode = RippleColorMode::Random;
        ds.idle = {0, 0, 0};
        ds.brush = RippleBrush::Fill;
        ds.shape = RippleShape::Jump;
        ds.speed = 10;
        ds.lifetime = 1;
        ds.fade = 0;
        ds.brightness = 1;
        ds.echo_count = 0;
        ds.impact_flash = false;
        ds.blend = RippleBlend::Max;
        ds.paint_idle = true;
        dart.SetSettings(ds);
        dart.Spawn(4, 0, 0.0, 7, LayoutBounds{}, true, 0.0f, 0.0f);
        Ripple d;
        Expect(dart.LastNonBlast(d), "spawned dart is LastNonBlast");
        RippleRGB arrival = RippleEngine::DartArrivalColor(ds, d);
        Expect(arrival.r == d.color.r && arrival.g == d.color.g && arrival.b == d.color.b,
               "dartArrivalColor random uses dart.color");
        dart.KeepOnlyBlasts();
        dart.SpawnBlast(4, 0, 1.0, 99, &d);
        RippleRGB blast = dart.Sample(4, 0, 1.0);
        Expect(std::fabs(blast.r - d.color.r) < 1.5f
            && std::fabs(blast.g - d.color.g) < 1.5f
            && std::fabs(blast.b - d.color.b) < 1.5f,
               "SpawnBlast fromDart uses dart.color, not a new seed");
        const RippleRGB seeded = RippleEngine::ColorForPress(ds, 1.0, 99);
        Expect(std::fabs(seeded.r - d.color.r) > 1.0f
            || std::fabs(seeded.g - d.color.g) > 1.0f
            || std::fabs(seeded.b - d.color.b) > 1.0f,
               "seed 99 would have been a different random colour");
    }

    {
        RippleEngine blast;
        RippleSettings bs = blast.GetSettings();
        bs.enabled = true;
        bs.color_mode = RippleColorMode::Solid;
        bs.solid = {255, 0, 0};
        bs.idle = {0, 0, 0};
        bs.brush = RippleBrush::Fill;
        bs.shape = RippleShape::Jump;
        bs.speed = 10;
        bs.lifetime = 1;
        bs.fade = 0;
        bs.brightness = 1;
        bs.echo_count = 0;
        bs.impact_flash = false;
        bs.blend = RippleBlend::Max;
        bs.paint_idle = true;
        bs.blast_size = 3.5f;
        blast.SetSettings(bs);
        blast.SpawnBlast(0, 0, 0.0, 1);
        /* expand = max(0.08, 3.5/10) = 0.35; fade 0 → snap after expand */
        RippleRGB live = blast.Sample(0, 0, 0.1);
        Expect(live.r > 200, "blast lights landing while expanding");
        RippleRGB after = blast.Sample(0, 0, 0.36);
        Expect(after.r < 1 && after.g < 1 && after.b < 1,
               "after blast life elapsed, landing is idle");
    }

    {
        RippleEngine dart;
        RippleSettings ds = dart.GetSettings();
        ds.enabled = true;
        ds.color_mode = RippleColorMode::Solid;
        ds.solid = {255, 0, 0};
        ds.idle = {0, 0, 0};
        ds.brush = RippleBrush::Fill;
        ds.shape = RippleShape::Jump;
        ds.speed = 10;
        ds.lifetime = 1;
        ds.fade = 0;
        ds.brightness = 1;
        ds.echo_count = 0;
        ds.impact_flash = false;
        ds.blend = RippleBlend::Max;
        ds.paint_idle = true;
        ds.trail_length = 0.0f;
        ds.blast_size = 3.5f;
        dart.SetSettings(ds);
        dart.Spawn(10, 0, 0.0, 1, LayoutBounds{}, true, 0.0f, 0.0f);
        RippleRGB head = dart.Sample(5, 0, 0.5);
        Expect(head.r > 200, "pre-blast dart head is on");
        dart.SpawnBlast(10, 0, 0.5, 2);
        RippleRGB skipped = dart.Sample(5, 0, 0.5);
        Expect(skipped.r < 1 && skipped.g < 1 && skipped.b < 1,
               "live blast skips dart sample");
    }

    if(g_fails)
    {
        std::fprintf(stderr, "%d failed\n", g_fails);
        return 1;
    }
    std::printf("hostless tests ok\n");
    return 0;
}
