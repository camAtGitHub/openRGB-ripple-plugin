/*---------------------------------------------------------*\
| OpenRGBRippleClient.cpp                                   |
|                                                           |
|   Standalone Windows client for OpenRGB SDK 5.            |
|   Hooks key-down and paints an Artemis-style ripple.      |
|                                                           |
|   cl /EHsc /O2 /DUNICODE /D_UNICODE OpenRGBRippleClient.cpp \
|      ws2_32.lib user32.lib                                |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#else
#error This client is a Win32 + Winsock program.
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/* Shared engine (header-only). Path works from repo root or sdk-client/. */
#if __has_include("../RippleEngine.h")
#include "../RippleEngine.h"
#include "../KeyMap.h"
#else
#include "RippleEngine.h"
#include "KeyMap.h"
#endif

struct KeyEvent
{
    uint32_t vk       = 0;
    uint32_t scan     = 0;
    bool     extended = false;
    bool     down     = true;
};

#include "RippleConfig.inc"

static const uint32_t PROTO = 5;

enum
{
    NET_PACKET_ID_REQUEST_CONTROLLER_COUNT = 0,
    NET_PACKET_ID_REQUEST_CONTROLLER_DATA  = 1,
    NET_PACKET_ID_REQUEST_PROTOCOL_VERSION = 40,
    NET_PACKET_ID_SET_CLIENT_NAME          = 50,
    RGBCONTROLLER_UPDATELEDS               = 1050,
    RGBCONTROLLER_SETCUSTOMMODE            = 1100,
};

enum
{
    ZONE_TYPE_SINGLE = 0,
    ZONE_TYPE_LINEAR = 1,
    ZONE_TYPE_MATRIX = 2,
};

enum
{
    DEVICE_TYPE_KEYBOARD = 5,
    DEVICE_TYPE_KEYPAD   = 19,
    DEVICE_TYPE_LAPTOP   = 20,
};

#pragma pack(push, 1)
struct NetHeader
{
    char     magic[4];
    uint32_t dev_idx;
    uint32_t pkt_id;
    uint32_t pkt_size;
};
#pragma pack(pop)

struct LedPos
{
    uint32_t    index;
    float       x;
    float       y;
    std::string name;
};

struct Device
{
    uint32_t             index = 0;
    int                  type  = 0;
    std::string          name;
    uint16_t             led_count = 0;
    uint16_t             color_count = 0;
    std::vector<LedPos>  leds;
    std::vector<std::string> all_names;
    std::vector<uint8_t> colors; /* 4 * color_count */
};

struct Reader
{
    const uint8_t* p;
    const uint8_t* end;
    bool ok = true;

    explicit Reader(const std::vector<uint8_t>& b)
        : p(b.data()), end(b.data() + b.size()) {}

    template<typename T>
    T U()
    {
        T v{};
        if(p + sizeof(T) > end)
        {
            ok = false;
            return v;
        }
        std::memcpy(&v, p, sizeof(T));
        p += sizeof(T);
        return v;
    }

    std::string Str()
    {
        const uint16_t n = U<uint16_t>();
        if(!ok || p + n > end)
        {
            ok = false;
            return {};
        }
        std::string s(reinterpret_cast<const char*>(p), n ? n - 1 : 0);
        p += n;
        return s;
    }

    void Skip(size_t n)
    {
        if(p + n > end)
        {
            ok = false;
            return;
        }
        p += n;
    }
};

static bool SendAll(SOCKET s, const void* data, int len)
{
    const char* p = static_cast<const char*>(data);
    int sent = 0;
    while(sent < len)
    {
        const int n = send(s, p + sent, len - sent, 0);
        if(n <= 0) return false;
        sent += n;
    }
    return true;
}

static bool RecvAll(SOCKET s, void* data, int len)
{
    char* p = static_cast<char*>(data);
    int got = 0;
    while(got < len)
    {
        const int n = recv(s, p + got, len - got, 0);
        if(n <= 0) return false;
        got += n;
    }
    return true;
}

static bool SendPkt(SOCKET s, uint32_t dev, uint32_t id, const void* payload, uint32_t size)
{
    NetHeader h{};
    h.magic[0] = 'O'; h.magic[1] = 'R'; h.magic[2] = 'G'; h.magic[3] = 'B';
    h.dev_idx  = dev;
    h.pkt_id   = id;
    h.pkt_size = size;
    if(!SendAll(s, &h, sizeof(h))) return false;
    if(size && payload)
    {
        return SendAll(s, payload, static_cast<int>(size));
    }
    return true;
}

static bool RecvPkt(SOCKET s, NetHeader& h, std::vector<uint8_t>& body)
{
    if(!RecvAll(s, &h, sizeof(h))) return false;
    if(std::memcmp(h.magic, "ORGB", 4) != 0) return false;
    body.resize(h.pkt_size);
    if(h.pkt_size == 0) return true;
    return RecvAll(s, body.data(), static_cast<int>(h.pkt_size));
}

static bool Request(SOCKET s, uint32_t dev, uint32_t id, const void* payload, uint32_t size,
                    NetHeader& rh, std::vector<uint8_t>& body)
{
    if(!SendPkt(s, dev, id, payload, size)) return false;
    return RecvPkt(s, rh, body);
}

static void SkipMode(Reader& r)
{
    r.Str();                 /* name */
    r.U<int32_t>();          /* value */
    r.U<uint32_t>();         /* flags */
    r.U<uint32_t>();         /* speed_min */
    r.U<uint32_t>();         /* speed_max */
    r.U<uint32_t>();         /* brightness_min  proto >= 3 */
    r.U<uint32_t>();         /* brightness_max */
    r.U<uint32_t>();         /* colors_min */
    r.U<uint32_t>();         /* colors_max */
    r.U<uint32_t>();         /* speed */
    r.U<uint32_t>();         /* brightness      proto >= 3 */
    r.U<uint32_t>();         /* direction */
    r.U<uint32_t>();         /* color_mode */
    const uint16_t n = r.U<uint16_t>();
    r.Skip(static_cast<size_t>(n) * 4);
}

static bool ParseDevice(uint32_t index, const std::vector<uint8_t>& body, Device& out)
{
    Reader r(body);
    const uint32_t declared = r.U<uint32_t>();
    out.index = index;
    out.type  = r.U<int32_t>();
    out.name  = r.Str();
    r.Str(); /* vendor proto>=1 */
    r.Str(); /* description */
    r.Str(); /* version */
    r.Str(); /* serial */
    r.Str(); /* location */

    const uint16_t nmodes = r.U<uint16_t>();
    r.U<int32_t>(); /* active_mode — required, all protocol versions */
    if(nmodes > 256)
    {
        std::fprintf(stderr, "  [%u] %s: implausible mode count %u (parse desync, offset %zu / %zu, declared %u)\n",
                     index, out.name.c_str(), nmodes, r.p - body.data(), body.size(), declared);
        return false;
    }
    for(uint16_t i = 0; i < nmodes && r.ok; i++) SkipMode(r);

    struct ZoneTmp
    {
        int      type;
        uint32_t start;
        uint32_t count;
        uint32_t w = 0, h = 0;
        std::vector<uint32_t> map;
        bool has_map = false;
    };
    std::vector<ZoneTmp> zones;
    uint32_t cursor = 0;

    const uint16_t nzones = r.U<uint16_t>();
    if(nzones > 128)
    {
        std::fprintf(stderr, "  [%u] %s: implausible zone count %u (offset %zu)\n",
                     index, out.name.c_str(), nzones, r.p - body.data());
        return false;
    }
    for(uint16_t z = 0; z < nzones && r.ok; z++)
    {
        r.Str();
        ZoneTmp zn;
        zn.type  = r.U<int32_t>();
        r.U<uint32_t>(); /* leds_min */
        r.U<uint32_t>(); /* leds_max */
        zn.count = r.U<uint32_t>();
        zn.start = cursor;
        cursor  += zn.count;
        const uint16_t mlen = r.U<uint16_t>();
        if(mlen)
        {
            zn.h = r.U<uint32_t>();
            zn.w = r.U<uint32_t>();
            const uint64_t cells = uint64_t(zn.w) * zn.h;
            if(!r.ok || cells > 4096)
            {
                std::fprintf(stderr, "  [%u] %s: bad matrix %ux%u\n",
                             index, out.name.c_str(), zn.w, zn.h);
                return false;
            }
            zn.map.resize(static_cast<size_t>(cells));
            for(uint32_t i = 0; i < cells && r.ok; i++) zn.map[i] = r.U<uint32_t>();
            zn.has_map = true;
        }
        const uint16_t nseg = r.U<uint16_t>(); /* proto >= 4 */
        if(nseg > 128) { r.ok = false; break; }
        for(uint16_t s = 0; s < nseg && r.ok; s++)
        {
            r.Str();
            r.U<int32_t>();
            r.U<uint32_t>();
            r.U<uint32_t>();
        }
        r.U<uint32_t>(); /* zone flags proto >= 5 */
        zones.push_back(std::move(zn));
    }

    const uint16_t nleds = r.U<uint16_t>();
    if(!r.ok || nleds > 2048)
    {
        std::fprintf(stderr, "  [%u] %s: LED parse failed (nleds=%u ok=%d offset %zu / %zu)\n",
                     index, out.name.c_str(), nleds, int(r.ok), r.p - body.data(), body.size());
        return false;
    }
    std::vector<std::string> led_names;
    led_names.reserve(nleds);
    for(uint16_t i = 0; i < nleds && r.ok; i++)
    {
        led_names.push_back(r.Str());
        r.U<uint32_t>(); /* led.value */
    }

    /* Tail is either proto-5 (alt names + flags + colors) or just colors.
       Razer Ornata V2 on 1.0rc3 is 132 LED names / 130 colors, no alt-names. */
    uint16_t ncols = nleds;
    const uint8_t* const save = r.p;
    const bool save_ok = r.ok;
    {
        const uint16_t nalt = r.U<uint16_t>();
        bool proto5 = r.ok && nalt < 512;
        if(proto5)
        {
            for(uint16_t i = 0; i < nalt && r.ok; i++) r.Str();
            r.U<uint32_t>();
            const uint16_t nc = r.U<uint16_t>();
            if(r.ok && nc <= 2048 && r.p + size_t(nc) * 4 <= r.end)
            {
                ncols = nc;
                r.Skip(size_t(nc) * 4);
            }
            else proto5 = false;
        }
        if(!proto5)
        {
            r.p = save;
            r.ok = save_ok;
            const uint16_t nc = r.U<uint16_t>();
            if(r.ok && nc <= 2048 && r.p + size_t(nc) * 4 <= r.end)
            {
                ncols = nc;
                r.Skip(size_t(nc) * 4);
            }
            else
            {
                r.p = save;
                r.ok = true;
                ncols = nleds;
            }
        }
    }

    out.led_count = nleds;
    out.color_count = ncols ? ncols : nleds;
    out.all_names = led_names;
    out.colors.assign(static_cast<size_t>(out.color_count) * 4, 0);
    out.leds.clear();
    bool used_matrix = false;
    for(const ZoneTmp& zn : zones)
    {
        if(zn.has_map && zn.type == ZONE_TYPE_MATRIX)
        {
            used_matrix = true;
            for(uint32_t y = 0; y < zn.h; y++)
            {
                for(uint32_t x = 0; x < zn.w; x++)
                {
                    const uint32_t idx = zn.map[y * zn.w + x];
                    if(idx == 0xFFFFFFFFu) continue;
                    LedPos lp;
                    lp.index = zn.start + idx;
                    lp.x = static_cast<float>(x);
                    lp.y = static_cast<float>(y);
                    if(lp.index < led_names.size()) lp.name = led_names[lp.index];
                    out.leds.push_back(lp);
                }
            }
        }
    }
    if(!used_matrix)
    {
        for(uint16_t i = 0; i < nleds; i++)
        {
            LedPos lp;
            lp.index = i;
            lp.x = static_cast<float>(i);
            lp.y = 0;
            if(i < led_names.size()) lp.name = led_names[i];
            out.leds.push_back(lp);
        }
    }
    return !out.leds.empty();
}

static bool IsKeyboard(const Device& d)
{
    if(d.type == DEVICE_TYPE_KEYBOARD
        || d.type == DEVICE_TYPE_KEYPAD
        || d.type == DEVICE_TYPE_LAPTOP)
        return true;
    /* Some boards report as unknown/accessory but still have Key: LEDs. */
    for(const LedPos& led : d.leds)
    {
        if(led.name.rfind("Key:", 0) == 0) return true;
    }
    return !d.leds.empty() && d.led_count > 20;
}

static LayoutBounds BoundsFromDevices(const std::vector<Device>& devices)
{
    LayoutBounds bounds;
    bool have = false;
    for(const Device& d : devices)
    {
        for(const LedPos& led : d.leds)
        {
            if(!have)
            {
                bounds.minX = bounds.maxX = led.x;
                bounds.minY = bounds.maxY = led.y;
                have = true;
            }
            else
            {
                bounds.minX = std::min(bounds.minX, led.x);
                bounds.maxX = std::max(bounds.maxX, led.x);
                bounds.minY = std::min(bounds.minY, led.y);
                bounds.maxY = std::max(bounds.maxY, led.y);
            }
        }
    }
    return bounds;
}

struct HookState
{
    std::mutex            mu;
    std::vector<KeyEvent> q;
    std::atomic<bool>     run{true};
};

static HookState* g_hook = nullptr;

static LRESULT CALLBACK LowLevel(int code, WPARAM wp, LPARAM lp)
{
    if(g_hook && code >= 0 && (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN))
    {
        const KBDLLHOOKSTRUCT* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);
        if(info && (info->flags & LLKHF_INJECTED) == 0)
        {
            KeyEvent ev;
            ev.vk = info->vkCode;
            ev.scan = info->scanCode;
            ev.extended = (info->flags & LLKHF_EXTENDED) != 0;
            ev.down = true;
            std::lock_guard<std::mutex> lock(g_hook->mu);
            g_hook->q.push_back(ev);
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

int RunRipple(int argc, char** argv)
{
    AppConfig cfg;
    const int parsed = ParseArgs(argc, argv, cfg);
    if(parsed != 1) return parsed == 0 ? 0 : parsed;


    WSADATA wsa;
    if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sock == INVALID_SOCKET)
    {
        std::fprintf(stderr, "socket failed\n");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg.port);
    inet_pton(AF_INET, cfg.host.c_str(), &addr.sin_addr);

    std::printf("Connecting to OpenRGB SDK %s:%u ...\n", cfg.host.c_str(), cfg.port);
    if(connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        std::fprintf(stderr,
            "Could not connect. Start OpenRGB, enable the SDK server (port 6742),\n"
            "and leave it running.\n");
        return 1;
    }

    NetHeader rh{};
    std::vector<uint8_t> body;
    uint32_t proto = PROTO;
    if(!Request(sock, 0, NET_PACKET_ID_REQUEST_PROTOCOL_VERSION, &proto, 4, rh, body)
       || body.size() < 4)
    {
        std::fprintf(stderr, "Protocol handshake failed\n");
        return 1;
    }
    uint32_t server_proto = 0;
    std::memcpy(&server_proto, body.data(), 4);
    std::printf("SDK protocol: client %u, server %u\n", PROTO, server_proto);

    const char* name = "OpenRGB Ripple";
    SendPkt(sock, 0, NET_PACKET_ID_SET_CLIENT_NAME, name, static_cast<uint32_t>(std::strlen(name) + 1));

    if(!Request(sock, 0, NET_PACKET_ID_REQUEST_CONTROLLER_COUNT, nullptr, 0, rh, body)
       || body.size() < 4)
    {
        std::fprintf(stderr, "Controller count failed\n");
        return 1;
    }
    uint32_t count = 0;
    std::memcpy(&count, body.data(), 4);
    std::printf("Controllers: %u\n", count);

    std::vector<Device> devices;
    for(uint32_t i = 0; i < count; i++)
    {
        uint32_t pv = PROTO;
        if(!Request(sock, i, NET_PACKET_ID_REQUEST_CONTROLLER_DATA, &pv, 4, rh, body))
        {
            continue;
        }
        Device d;
        if(!ParseDevice(i, body, d))
        {
            std::printf("  [%u] parse failed\n", i);
            continue;
        }
        std::printf("  [%u] %s (type %d, %u LEDs, %u colors, %zu mapped)\n",
                    i, d.name.c_str(), d.type, d.led_count, d.color_count, d.leds.size());
        if(IsKeyboard(d) && !d.leds.empty())
        {
            SendPkt(sock, i, RGBCONTROLLER_SETCUSTOMMODE, nullptr, 0);
            devices.push_back(std::move(d));
        }
    }

    if(devices.empty())
    {
        std::fprintf(stderr, "No keyboard devices with LEDs found.\n");
        return 1;
    }

    if(cfg.debug)
    {
        for(const Device& d : devices)
        {
            std::printf("\n== Keymap: %s  leds=%u colors=%u mapped=%zu ==\n",
                        d.name.c_str(), d.led_count, d.color_count, d.leds.size());
            std::printf("idx   x     y     OpenRGB LED name\n");
            std::vector<char> used(d.all_names.size(), 0);
            for(const LedPos& led : d.leds)
            {
                std::printf("%4u  %5.1f %5.1f  %s\n",
                            led.index, led.x, led.y,
                            led.name.empty() ? "(unnamed)" : led.name.c_str());
                if(led.index < used.size()) used[led.index] = 1;
            }
            bool any_unmapped = false;
            for(size_t i = 0; i < d.all_names.size(); i++)
            {
                if(!used[i])
                {
                    if(!any_unmapped)
                    {
                        std::printf("\nUnmapped LEDs (no matrix cell):\n");
                        any_unmapped = true;
                    }
                    std::printf("%4zu               %s\n", i,
                                d.all_names[i].empty() ? "(unnamed)" : d.all_names[i].c_str());
                }
            }
        }
        std::printf("\n== VK aliases this client looks for ==\n");
        for(const KeyAlias& a : KeyMap::Aliases())
        {
            std::printf("  vk=0x%02X ->", a.vk);
            for(const std::string& n : a.names) std::printf("  %s", n.c_str());
            std::printf("\n");
        }
        std::printf("\n");
    }

    Sleep(80); /* let OpenRGB apply Direct before the first frame */

    HookState hook;
    g_hook = &hook;
    std::atomic<bool> hook_ready{false};
    std::atomic<DWORD> hook_err{0};
    std::thread pump([&]()
    {
        /* WH_KEYBOARD_LL callbacks are delivered to THIS thread's queue. */
        HHOOK hh = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevel, GetModuleHandleW(nullptr), 0);
        if(!hh)
        {
            hook_err = GetLastError();
            hook_ready = true;
            hook.run = false;
            return;
        }
        hook_ready = true;
        MSG msg;
        while(hook.run)
        {
            const BOOL gm = GetMessageW(&msg, nullptr, 0, 0);
            if(gm <= 0) break;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        UnhookWindowsHookEx(hh);
    });
    while(!hook_ready) Sleep(1);
    if(hook_err)
    {
        std::fprintf(stderr, "SetWindowsHookEx failed (%lu)\n", hook_err.load());
        if(pump.joinable()) pump.join();
        return 1;
    }
    std::printf("Key hook armed. Type to ripple. Ctrl+C to quit.\n");

    RippleEngine engine;
    engine.SetSettings(cfg.fx);
    {
        const char* brush =
            cfg.fx.brush == RippleBrush::Fill ? "fill" :
            cfg.fx.brush == RippleBrush::Soft ? "soft" : "ring";
        const char* shape =
            cfg.fx.shape == RippleShape::Square ? "square" :
            cfg.fx.shape == RippleShape::Axis   ? "axis" :
            cfg.fx.shape == RippleShape::Sweep  ? "sweep" :
            cfg.fx.shape == RippleShape::Jump   ? "dart" : "circle";
        const char* color =
            cfg.fx.color_mode == RippleColorMode::Solid ? "solid" :
            cfg.fx.color_mode == RippleColorMode::Random ? "random" : "rainbow";
        std::printf("Brush %s  shape %s  color %s  speed %.1f  thickness %.2f  life %.2f  echoes %d\n",
                    brush, shape, color, cfg.fx.speed, cfg.fx.thickness, cfg.fx.lifetime, cfg.fx.echo_count);
    }
    uint32_t seed = 1;
    bool have_last = false;
    float last_x = 0;
    float last_y = 0;
    double lastPressAt = 0;
    bool pending_blast = false;
    const LayoutBounds bounds = BoundsFromDevices(devices);
    auto t0 = std::chrono::steady_clock::now();
    auto now_s = [&]()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    };

    /* Prove UPDATE_LEDS is accepted: one teal flash, then idle. */
    engine.Spawn(
        devices[0].leds[devices[0].leds.size() / 2].x,
        devices[0].leds[devices[0].leds.size() / 2].y,
        0.0, 1, bounds);

    while(hook.run)
    {
        std::vector<KeyEvent> evs;
        {
            std::lock_guard<std::mutex> lock(hook.mu);
            evs.swap(hook.q);
        }
        const double now = now_s();
        const RippleSettings s = engine.GetSettings();
        const bool jump = s.shape == RippleShape::Jump;
        auto spawn_at = [&](float x, float y)
        {
            pending_blast = jump;
            if(jump)
            {
                engine.DropBlasts();
            }
            engine.Spawn(x, y, now, seed++, bounds, have_last, last_x, last_y);
            last_x = x;
            last_y = y;
            lastPressAt = now;
            have_last = true;
        };
        for(const KeyEvent& ev : evs)
        {
            const auto names = KeyMap::NamesForVirtualKey(ev.vk, ev.scan, ev.extended);
            bool hit = false;
            std::string hit_name;
            for(const Device& d : devices)
            {
                for(const LedPos& led : d.leds)
                {
                    if(KeyMap::NameMatches(led.name, names))
                    {
                        spawn_at(led.x, led.y);
                        hit = true;
                        hit_name = led.name;
                        break;
                    }
                }
                if(hit) break;
            }
            if(!hit && !devices.empty() && !devices[0].leds.empty())
            {
                /* Matrix hole — still spawn at nearest mapped key so something shows. */
                const LedPos& led = devices[0].leds.front();
                spawn_at(led.x, led.y);
                hit_name = led.name + " (fallback)";
            }
            if(cfg.debug)
            {
                std::printf("  key vk=0x%02X -> %s\n", ev.vk,
                            hit_name.empty() ? "?" : hit_name.c_str());
            }
        }

        if(jump && pending_blast && have_last
           && now - lastPressAt >= static_cast<double>(s.lifetime))
        {
            Ripple lastDart;
            const bool haveDart = engine.LastNonBlast(lastDart);
            engine.KeepOnlyBlasts();
            engine.SpawnBlast(last_x, last_y, now, seed++,
                              haveDart ? &lastDart : nullptr);
            pending_blast = false;
        }

        engine.Prune(now);
        for(Device& d : devices)
        {
            const RippleRGB idle = cfg.fx.idle;
            for(size_t i = 0; i + 3 < d.colors.size(); i += 4)
            {
                d.colors[i + 0] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, idle.r)));
                d.colors[i + 1] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, idle.g)));
                d.colors[i + 2] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, idle.b)));
                d.colors[i + 3] = 0;
            }
            for(const LedPos& led : d.leds)
            {
                if(led.index >= d.color_count) continue;
                const RippleRGB c = engine.Sample(led.x, led.y, now);
                uint8_t* p = &d.colors[static_cast<size_t>(led.index) * 4];
                p[0] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, c.r)));
                p[1] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, c.g)));
                p[2] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, c.b)));
                p[3] = 0;
            }
            const uint16_t n = d.color_count;
            const uint32_t payload_size = 4 + 2 + static_cast<uint32_t>(n) * 4;
            std::vector<uint8_t> pkt(payload_size);
            std::memcpy(pkt.data(), &payload_size, 4);
            std::memcpy(pkt.data() + 4, &n, 2);
            if(n)
            {
                std::memcpy(pkt.data() + 6, d.colors.data(), static_cast<size_t>(n) * 4);
            }
            if(!SendPkt(sock, d.index, RGBCONTROLLER_UPDATELEDS, pkt.data(), payload_size))
            {
                std::fprintf(stderr, "Lost connection to OpenRGB.\n");
                hook.run = false;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    hook.run = false;
    PostThreadMessageW(GetThreadId(pump.native_handle()), WM_QUIT, 0, 0);
    if(pump.joinable()) pump.join();
    closesocket(sock);
    WSACleanup();
    return 0;
}

#ifndef OPENRGB_RIPPLE_DLL
int main(int argc, char** argv)
{
    return RunRipple(argc, argv);
}
#else
extern "C" __declspec(dllexport)
void CALLBACK StartA(HWND, HINSTANCE, LPSTR cmd, int)
{
    AllocConsole();
    FILE* fp = nullptr;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    int argc = 0;
    wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> store;
    std::vector<char*> argv;
    store.emplace_back("OpenRGBRipple.dll");
    if(cmd && *cmd)
    {
        /* rundll32 appends only the extra args after the export name. */
        std::string extra = cmd;
        store.push_back(extra);
    }
    if(wargv)
    {
        store.clear();
        for(int i = 0; i < argc; i++)
        {
            char buf[1024];
            WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, buf, 1024, nullptr, nullptr);
            store.emplace_back(buf);
        }
        LocalFree(wargv);
    }
    argv.reserve(store.size());
    for(auto& s : store) argv.push_back(s.data());
    RunRipple(static_cast<int>(argv.size()), argv.data());
}

extern "C" __declspec(dllexport)
void CALLBACK StartW(HWND hwnd, HINSTANCE inst, LPWSTR, int show)
{
    StartA(hwnd, inst, nullptr, show);
}

extern "C" __declspec(dllexport)
void CALLBACK Start(HWND hwnd, HINSTANCE inst, LPSTR cmd, int show)
{
    StartA(hwnd, inst, cmd, show);
}

BOOL APIENTRY DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if(reason == DLL_PROCESS_ATTACH)
    {
        /* Do not start sockets/hooks here — loader lock. Use rundll32 Start. */
    }
    return TRUE;
}
#endif

