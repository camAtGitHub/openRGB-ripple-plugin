#include "../RippleEngine.h"
#include "../KeyMap.h"
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

    if(g_fails)
    {
        std::fprintf(stderr, "%d failed\n", g_fails);
        return 1;
    }
    std::printf("hostless tests ok\n");
    return 0;
}
