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
        "  --color rainbow|solid|random\n"
        "  --solid <RRGGBB>     hex color when --color solid\n"
        "  --speed <n>          wave speed, keys/sec      (default 14)\n"
        "  --thickness <n>      ring width                (default 1.15)\n"
        "  --lifetime <n>       seconds                   (default 1.15)\n"
        "  --fade <n>           fade curve                (default 1.35)\n"
        "  --echoes <n>         extra rings               (default 1)\n"
        "  --echo-delay <n>     seconds between echoes    (default 0.12)\n"
        "  --brightness <n>     0..1                      (default 1)\n"
        "  --blend over|add     over = ripple on top      (default over)\n"
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
    if(JsonExtract(j, "fade", v)) cfg.fx.fade_power = std::strtof(v.c_str(), nullptr);
    if(JsonExtract(j, "echoes", v)) cfg.fx.echo_count = std::atoi(v.c_str());
    if(JsonExtract(j, "echo_delay", v)) cfg.fx.echo_delay = std::strtof(v.c_str(), nullptr);
    if(JsonExtract(j, "brightness", v)) cfg.fx.brightness = std::strtof(v.c_str(), nullptr);
    if(JsonExtract(j, "blend", v))
        cfg.fx.blend = (v == "add") ? RippleBlend::Add : RippleBlend::Max;
    if(JsonExtract(j, "impact", v))
        cfg.fx.impact_flash = !(v == "false" || v == "0");
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
    else if(a == "--fade") cfg.fx.fade_power = std::strtof(v, nullptr);
    else if(a == "--echoes") cfg.fx.echo_count = std::atoi(v);
    else if(a == "--echo-delay") cfg.fx.echo_delay = std::strtof(v, nullptr);
    else if(a == "--brightness") cfg.fx.brightness = std::strtof(v, nullptr);
    else if(a == "--blend")
    {
        cfg.fx.blend = (std::string(v) == "add") ? RippleBlend::Add : RippleBlend::Max;
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
