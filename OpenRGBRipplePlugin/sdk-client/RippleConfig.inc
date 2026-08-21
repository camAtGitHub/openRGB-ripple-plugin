struct AppConfig
{
    std::string     host = "127.0.0.1";
    uint16_t        port = 6742;
    std::string     config_path;
    RippleSettings  fx;
    bool            debug = false;
};

static void PrintHelp()
{
    std::printf(
        "OpenRGB Ripple  (SDK client)\n"
        "\n"
        "Usage: OpenRGBRipple.exe [options]\n"
        "\n"
        "Connection\n"
        "  --host <ip>          OpenRGB SDK host          (default 127.0.0.1)\n"
        "  --port <n>           OpenRGB SDK port          (default 6742)\n"
        "  --config <file>      JSON settings file\n"
        "\n"
        "Brush  (Artemis Key Press)\n"
        "  --brush ring|fill|soft\n"
        "  --shape circle|square|axis|sweep|jump\n"
        "                      Circle / Square / Row/Col / Sweep / Dart\n"
        "  --color rainbow|solid|random\n"
        "  --solid <RRGGBB>     hex color when --color solid\n"
        "  --speed <n>          wave speed, keys/sec      (default 14)\n"
        "  --thickness <n>      ring width                (default 1.15)\n"
        "  --lifetime <n>       max expand seconds, min 0.05 (default 1.15)\n"
        "  --fade <n>           seconds to retract; 0 = snap off (default 1.0)\n"
        "  --echoes <n>         extra rings               (default 1)\n"
        "  --echo-delay <n>     seconds between echoes    (default 0.12)\n"
        "  --brightness <n>     0..1                      (default 1)\n"
        "  --jitter <n>         Row/Col + Sweep short-way chance 0..1 (default 0.18)\n"
        "  --span <n>           Sweep lateral 0..1        (default 1)\n"
        "  --trail <n>          Dart trail length         (default 2.5)\n"
        "  --blast-size <n>     Dart explosion radius     (default 3.5)\n"
        "  --blast-shape circle|square                    (default circle)\n"
        "  --blend over|add|xor|screen|overlay|dodge|burn|exclusion\n"
        "                      Over, Add, XOR, Screen, Overlay, Color Dodge, Color Burn, Exclusion\n"
        "                      also color-dodge / color-burn (default over)\n"
        "  --impact / --no-impact   flash the pressed key (default on)\n"
        "  --idle <RRGGBB>      background color\n"
        "  --debug              dump LED keymap and key hits\n"
        "  --help\n"
        "\n"
        "If --config is omitted, OpenRGBRipple.json next to the exe is loaded.\n"
        "CLI flags override the file.\n");
}

static bool HexRgb(const std::string& hex_in, RippleRGB& out)
{
    std::string hex = hex_in;
    if(!hex.empty() && hex[0] == '#') hex.erase(0, 1);
    if(hex.size() != 6) return false;
    char* end = nullptr;
    const unsigned long v = std::strtoul(hex.c_str(), &end, 16);
    if(!end || *end) return false;
    out.r = float((v >> 16) & 0xFF);
    out.g = float((v >> 8) & 0xFF);
    out.b = float(v & 0xFF);
    return true;
}

static std::string NormToken(const std::string& s)
{
    std::string o;
    o.reserve(s.size());
    for(unsigned char c : s)
    {
        if(c == ' ' || c == '_' || c == '-' || c == '/') continue;
        if(c >= 'A' && c <= 'Z') o.push_back(static_cast<char>(c - 'A' + 'a'));
        else o.push_back(static_cast<char>(c));
    }
    return o;
}

static bool TokenIsInt(const std::string& s, int& out)
{
    if(s.empty()) return false;
    char* end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 10);
    if(!end || end == s.c_str() || *end) return false;
    out = static_cast<int>(v);
    return true;
}

static bool ParseBlend(const std::string& raw, RippleBlend& out)
{
    int n = 0;
    if(TokenIsInt(raw, n))
    {
        if(n >= 0 && n < RippleBlendCount)
        {
            out = static_cast<RippleBlend>(n);
            return true;
        }
        return false;
    }
    const std::string t = NormToken(raw);
    if(t == "over" || t == "max") out = RippleBlend::Max;
    else if(t == "add" || t == "plus") out = RippleBlend::Add;
    else if(t == "xor") out = RippleBlend::Xor;
    else if(t == "screen") out = RippleBlend::Screen;
    else if(t == "overlay") out = RippleBlend::Overlay;
    else if(t == "dodge" || t == "colordodge") out = RippleBlend::ColorDodge;
    else if(t == "burn" || t == "colorburn") out = RippleBlend::ColorBurn;
    else if(t == "exclusion") out = RippleBlend::Exclusion;
    else return false;
    return true;
}

static bool ParseShape(const std::string& raw, RippleShape& out)
{
    int n = 0;
    if(TokenIsInt(raw, n))
    {
        if(n >= 0 && n <= 4)
        {
            out = static_cast<RippleShape>(n);
            return true;
        }
        return false;
    }
    const std::string t = NormToken(raw);
    if(t == "circle") out = RippleShape::Circle;
    else if(t == "square") out = RippleShape::Square;
    else if(t == "axis" || t == "row" || t == "col" || t == "rowcol")
        out = RippleShape::Axis;
    else if(t == "sweep") out = RippleShape::Sweep;
    else if(t == "jump" || t == "dart") out = RippleShape::Jump;
    else return false;
    return true;
}

static bool ParseBlastShape(const std::string& raw, RippleBlastShape& out)
{
    int n = 0;
    if(TokenIsInt(raw, n))
    {
        if(n == 0 || n == 1)
        {
            out = static_cast<RippleBlastShape>(n);
            return true;
        }
        return false;
    }
    const std::string t = NormToken(raw);
    if(t == "circle") out = RippleBlastShape::Circle;
    else if(t == "square") out = RippleBlastShape::Square;
    else return false;
    return true;
}

static bool JsonExtract(const std::string& j, const char* key, std::string& value)
{
    const std::string pat = std::string("\"") + key + "\"";
    auto pos = j.find(pat);
    if(pos == std::string::npos) return false;
    pos = j.find(':', pos + pat.size());
    if(pos == std::string::npos) return false;
    pos++;
    while(pos < j.size() && (j[pos] == ' ' || j[pos] == '\t')) pos++;
    if(pos >= j.size()) return false;
    if(j[pos] == '"')
    {
        const auto end = j.find('"', pos + 1);
        if(end == std::string::npos) return false;
        value = j.substr(pos + 1, end - pos - 1);
        return true;
    }
    auto end = pos;
    while(end < j.size() && j[end] != ',' && j[end] != '}' && j[end] != '\n') end++;
    value = j.substr(pos, end - pos);
    while(!value.empty() && (value.back() == ' ' || value.back() == '\r')) value.pop_back();
    return !value.empty();
}

static void ApplyJson(const std::string& j, AppConfig& cfg)
{
    std::string v;
    if(JsonExtract(j, "host", v)) cfg.host = v;
    if(JsonExtract(j, "port", v)) cfg.port = static_cast<uint16_t>(std::atoi(v.c_str()));
    if(JsonExtract(j, "brush", v))
    {
        if(v == "fill") cfg.fx.brush = RippleBrush::Fill;
        else if(v == "soft") cfg.fx.brush = RippleBrush::Soft;
        else cfg.fx.brush = RippleBrush::Ring;
    }
    if(JsonExtract(j, "color", v))
    {
        if(v == "solid") cfg.fx.color_mode = RippleColorMode::Solid;
        else if(v == "random") cfg.fx.color_mode = RippleColorMode::Random;
        else cfg.fx.color_mode = RippleColorMode::Rainbow;
    }
    if(JsonExtract(j, "solid", v)) HexRgb(v, cfg.fx.solid);
    if(JsonExtract(j, "idle", v)) HexRgb(v, cfg.fx.idle);
    if(JsonExtract(j, "speed", v)) cfg.fx.speed = std::strtof(v.c_str(), nullptr);
    if(JsonExtract(j, "thickness", v)) cfg.fx.thickness = std::strtof(v.c_str(), nullptr);
    if(JsonExtract(j, "lifetime", v)) cfg.fx.lifetime = std::strtof(v.c_str(), nullptr);
    if(JsonExtract(j, "fade", v)) cfg.fx.fade = std::strtof(v.c_str(), nullptr);
    if(JsonExtract(j, "echoes", v)) cfg.fx.echo_count = std::atoi(v.c_str());
    if(JsonExtract(j, "echo_delay", v)) cfg.fx.echo_delay = std::strtof(v.c_str(), nullptr);
    if(JsonExtract(j, "brightness", v)) cfg.fx.brightness = std::strtof(v.c_str(), nullptr);
    if(JsonExtract(j, "blend", v))
    {
        if(!ParseBlend(v, cfg.fx.blend)) cfg.fx.blend = RippleBlend::Max;
    }
    if(JsonExtract(j, "impact", v))
        cfg.fx.impact_flash = !(v == "false" || v == "0");
    if(JsonExtract(j, "shape", v))
    {
        RippleShape sh;
        if(ParseShape(v, sh)) cfg.fx.shape = sh;
    }
    if(JsonExtract(j, "axis_jitter", v)) cfg.fx.axis_jitter = std::strtof(v.c_str(), nullptr);
    if(JsonExtract(j, "sweep_span", v)) cfg.fx.sweep_span = std::strtof(v.c_str(), nullptr);
    if(JsonExtract(j, "trail_length", v)) cfg.fx.trail_length = std::strtof(v.c_str(), nullptr);
    if(JsonExtract(j, "blast_size", v)) cfg.fx.blast_size = std::strtof(v.c_str(), nullptr);
    if(JsonExtract(j, "blast_shape", v))
    {
        RippleBlastShape bs;
        if(ParseBlastShape(v, bs)) cfg.fx.blast_shape = bs;
    }
}

static std::string ExeDir()
{
    char buf[MAX_PATH]{};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p = buf;
    const auto slash = p.find_last_of("\\/");
    return slash == std::string::npos ? std::string(".") : p.substr(0, slash);
}

static bool LoadFile(const std::string& path, std::string& out)
{
    FILE* f = nullptr;
    if(fopen_s(&f, path.c_str(), "rb") != 0 || !f) return false;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? static_cast<size_t>(n) : 0);
    if(!out.empty()) fread(out.data(), 1, out.size(), f);
    fclose(f);
    return true;
}

static const char* NextArg(int argc, char** argv, int& i)
{
    if(i + 1 >= argc) return nullptr;
    return argv[++i];
}

static void ApplyFlag(AppConfig& cfg, const std::string& a, const char* v)
{
    if(!v) return;
    if(a == "--host") cfg.host = v;
    else if(a == "--port") cfg.port = static_cast<uint16_t>(std::atoi(v));
    else if(a == "--brush")
    {
        if(std::string(v) == "fill") cfg.fx.brush = RippleBrush::Fill;
        else if(std::string(v) == "soft") cfg.fx.brush = RippleBrush::Soft;
        else cfg.fx.brush = RippleBrush::Ring;
    }
    else if(a == "--shape")
    {
        RippleShape sh;
        if(ParseShape(v, sh)) cfg.fx.shape = sh;
        else cfg.fx.shape = RippleShape::Circle;
    }
    else if(a == "--color")
    {
        if(std::string(v) == "solid") cfg.fx.color_mode = RippleColorMode::Solid;
        else if(std::string(v) == "random") cfg.fx.color_mode = RippleColorMode::Random;
        else cfg.fx.color_mode = RippleColorMode::Rainbow;
    }
    else if(a == "--solid") HexRgb(v, cfg.fx.solid);
    else if(a == "--idle") HexRgb(v, cfg.fx.idle);
    else if(a == "--speed") cfg.fx.speed = std::strtof(v, nullptr);
    else if(a == "--thickness") cfg.fx.thickness = std::strtof(v, nullptr);
    else if(a == "--lifetime") cfg.fx.lifetime = std::strtof(v, nullptr);
    else if(a == "--fade") cfg.fx.fade = std::strtof(v, nullptr);
    else if(a == "--echoes") cfg.fx.echo_count = std::atoi(v);
    else if(a == "--echo-delay") cfg.fx.echo_delay = std::strtof(v, nullptr);
    else if(a == "--brightness") cfg.fx.brightness = std::strtof(v, nullptr);
    else if(a == "--jitter") cfg.fx.axis_jitter = std::strtof(v, nullptr);
    else if(a == "--span") cfg.fx.sweep_span = std::strtof(v, nullptr);
    else if(a == "--trail") cfg.fx.trail_length = std::strtof(v, nullptr);
    else if(a == "--blast-size") cfg.fx.blast_size = std::strtof(v, nullptr);
    else if(a == "--blast-shape")
    {
        RippleBlastShape bs;
        if(ParseBlastShape(v, bs)) cfg.fx.blast_shape = bs;
        else cfg.fx.blast_shape = RippleBlastShape::Circle;
    }
    else if(a == "--blend")
    {
        if(!ParseBlend(v, cfg.fx.blend)) cfg.fx.blend = RippleBlend::Max;
    }
}

static int ParseArgs(int argc, char** argv, AppConfig& cfg)
{
    std::string cli_config;
    for(int i = 1; i < argc; i++)
    {
        const std::string a = argv[i];
        if(a == "--help" || a == "-h") { PrintHelp(); return 0; }
        if(a == "--debug") { cfg.debug = true; continue; }
        if(a == "--impact") { cfg.fx.impact_flash = true; continue; }
        if(a == "--no-impact") { cfg.fx.impact_flash = false; continue; }
        if(a == "--config")
        {
            const char* v = NextArg(argc, argv, i);
            if(!v) { std::fprintf(stderr, "Missing value for --config\n"); return 2; }
            cli_config = v;
            continue;
        }
        if(a.rfind("--", 0) == 0)
        {
            const char* v = NextArg(argc, argv, i);
            if(!v) { std::fprintf(stderr, "Missing value for %s\n", a.c_str()); return 2; }
            ApplyFlag(cfg, a, v);
            continue;
        }
        std::fprintf(stderr, "Unknown option: %s  (try --help)\n", a.c_str());
        return 2;
    }

    std::string path = cli_config;
    if(path.empty())
    {
        const std::string beside = ExeDir() + "\\OpenRGBRipple.json";
        if(GetFileAttributesA(beside.c_str()) != INVALID_FILE_ATTRIBUTES) path = beside;
        else if(GetFileAttributesA("OpenRGBRipple.json") != INVALID_FILE_ATTRIBUTES)
            path = "OpenRGBRipple.json";
    }
    if(!path.empty())
    {
        std::string body;
        if(!LoadFile(path, body))
        {
            std::fprintf(stderr, "Could not read config %s\n", path.c_str());
            return 2;
        }
        AppConfig file_cfg;
        ApplyJson(body, file_cfg);
        cfg = file_cfg;
        cfg.config_path = path;
        for(int i = 1; i < argc; i++)
        {
            const std::string a = argv[i];
            if(a == "--impact") { cfg.fx.impact_flash = true; continue; }
            if(a == "--debug") { cfg.debug = true; continue; }
            if(a == "--no-impact") { cfg.fx.impact_flash = false; continue; }
            if(a == "--config") { ++i; continue; }
            if(a == "--help" || a == "-h") continue;
            if(a.rfind("--", 0) == 0)
            {
                const char* v = NextArg(argc, argv, i);
                ApplyFlag(cfg, a, v);
            }
        }
        std::printf("Config: %s\n", path.c_str());
    }
    return 1;
}
