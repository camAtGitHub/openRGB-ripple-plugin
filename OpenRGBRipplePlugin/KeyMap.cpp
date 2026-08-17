/*---------------------------------------------------------*\
| KeyMap.cpp                                                |
\*---------------------------------------------------------*/

#include "KeyMap.h"
#include <algorithm>
#include <cctype>

static std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

const std::vector<KeyAlias>& KeyMap::Aliases()
{
    static const std::vector<KeyAlias> k = {
#ifdef _WIN32
        { VK_ESCAPE,      {"Key: Escape"} },
        { VK_F1,          {"Key: F1"} },
        { VK_F2,          {"Key: F2"} },
        { VK_F3,          {"Key: F3"} },
        { VK_F4,          {"Key: F4"} },
        { VK_F5,          {"Key: F5"} },
        { VK_F6,          {"Key: F6"} },
        { VK_F7,          {"Key: F7"} },
        { VK_F8,          {"Key: F8"} },
        { VK_F9,          {"Key: F9"} },
        { VK_F10,         {"Key: F10"} },
        { VK_F11,         {"Key: F11"} },
        { VK_F12,         {"Key: F12"} },
        { VK_SNAPSHOT,    {"Key: Print Screen"} },
        { VK_SCROLL,      {"Key: Scroll Lock"} },
        { VK_PAUSE,       {"Key: Pause/Break"} },
        { VK_OEM_3,       {"Key: `"} },
        { '1',            {"Key: 1"} },
        { '2',            {"Key: 2"} },
        { '3',            {"Key: 3"} },
        { '4',            {"Key: 4"} },
        { '5',            {"Key: 5"} },
        { '6',            {"Key: 6"} },
        { '7',            {"Key: 7"} },
        { '8',            {"Key: 8"} },
        { '9',            {"Key: 9"} },
        { '0',            {"Key: 0"} },
        { VK_OEM_MINUS,   {"Key: -"} },
        { VK_OEM_PLUS,    {"Key: =", "Key: +"} },
        { VK_BACK,        {"Key: Backspace"} },
        { VK_INSERT,      {"Key: Insert"} },
        { VK_HOME,        {"Key: Home"} },
        { VK_PRIOR,       {"Key: Page Up"} },
        { VK_TAB,         {"Key: Tab"} },
        { 'Q',            {"Key: Q"} },
        { 'W',            {"Key: W"} },
        { 'E',            {"Key: E"} },
        { 'R',            {"Key: R"} },
        { 'T',            {"Key: T"} },
        { 'Y',            {"Key: Y"} },
        { 'U',            {"Key: U"} },
        { 'I',            {"Key: I"} },
        { 'O',            {"Key: O"} },
        { 'P',            {"Key: P"} },
        { VK_OEM_4,       {"Key: ["} },
        { VK_OEM_6,       {"Key: ]"} },
        { VK_OEM_5,       {"Key: \\", "Key: \\ (ANSI)", "Key: \\ (ISO)"} },
        { VK_DELETE,      {"Key: Delete"} },
        { VK_END,         {"Key: End"} },
        { VK_NEXT,        {"Key: Page Down"} },
        { VK_CAPITAL,     {"Key: Caps Lock"} },
        { 'A',            {"Key: A"} },
        { 'S',            {"Key: S"} },
        { 'D',            {"Key: D"} },
        { 'F',            {"Key: F"} },
        { 'G',            {"Key: G"} },
        { 'H',            {"Key: H"} },
        { 'J',            {"Key: J"} },
        { 'K',            {"Key: K"} },
        { 'L',            {"Key: L"} },
        { VK_OEM_1,       {"Key: ;"} },
        { VK_OEM_7,       {"Key: '"} },
        { VK_RETURN,      {"Key: Enter", "Key: Enter (ISO)"} },
        { VK_LSHIFT,      {"Key: Left Shift"} },
        { VK_RSHIFT,      {"Key: Right Shift"} },
        { 'Z',            {"Key: Z"} },
        { 'X',            {"Key: X"} },
        { 'C',            {"Key: C"} },
        { 'V',            {"Key: V"} },
        { 'B',            {"Key: B"} },
        { 'N',            {"Key: N"} },
        { 'M',            {"Key: M"} },
        { VK_OEM_COMMA,   {"Key: ,"} },
        { VK_OEM_PERIOD,  {"Key: ."} },
        { VK_OEM_2,       {"Key: /"} },
        { VK_UP,          {"Key: Up Arrow"} },
        { VK_DOWN,        {"Key: Down Arrow"} },
        { VK_LEFT,        {"Key: Left Arrow"} },
        { VK_RIGHT,       {"Key: Right Arrow"} },
        { VK_LCONTROL,    {"Key: Left Control"} },
        { VK_RCONTROL,    {"Key: Right Control"} },
        { VK_LWIN,        {"Key: Left Windows"} },
        { VK_RWIN,        {"Key: Right Windows"} },
        { VK_LMENU,       {"Key: Left Alt"} },
        { VK_RMENU,       {"Key: Right Alt"} },
        { VK_SPACE,       {"Key: Space"} },
        { VK_APPS,        {"Key: Menu"} },
        { VK_NUMLOCK,     {"Key: Num Lock"} },
        { VK_DIVIDE,      {"Key: Number Pad /"} },
        { VK_MULTIPLY,    {"Key: Number Pad *"} },
        { VK_SUBTRACT,    {"Key: Number Pad -"} },
        { VK_ADD,         {"Key: Number Pad +"} },
        { VK_DECIMAL,     {"Key: Number Pad ."} },
        { VK_NUMPAD0,     {"Key: Number Pad 0"} },
        { VK_NUMPAD1,     {"Key: Number Pad 1"} },
        { VK_NUMPAD2,     {"Key: Number Pad 2"} },
        { VK_NUMPAD3,     {"Key: Number Pad 3"} },
        { VK_NUMPAD4,     {"Key: Number Pad 4"} },
        { VK_NUMPAD5,     {"Key: Number Pad 5"} },
        { VK_NUMPAD6,     {"Key: Number Pad 6"} },
        { VK_NUMPAD7,     {"Key: Number Pad 7"} },
        { VK_NUMPAD8,     {"Key: Number Pad 8"} },
        { VK_NUMPAD9,     {"Key: Number Pad 9"} },
        { VK_MEDIA_PLAY_PAUSE, {"Key: Media Play/Pause"} },
        { VK_MEDIA_PREV_TRACK, {"Key: Media Previous"} },
        { VK_MEDIA_NEXT_TRACK, {"Key: Media Next"} },
        { VK_MEDIA_STOP,       {"Key: Media Stop"} },
        { VK_VOLUME_MUTE,      {"Key: Media Mute"} },
        { VK_VOLUME_DOWN,      {"Key: Media Volume -"} },
        { VK_VOLUME_UP,        {"Key: Media Volume +"} },
#endif
    };
    return k;
}

#ifdef _WIN32
uint32_t KeyMap::NormalizeVk(uint32_t vk, uint32_t /*scan*/, bool extended)
{
    switch(vk)
    {
        case VK_SHIFT:
            return extended ? VK_RSHIFT : VK_LSHIFT;
        case VK_CONTROL:
            return extended ? VK_RCONTROL : VK_LCONTROL;
        case VK_MENU:
            return extended ? VK_RMENU : VK_LMENU;
        case VK_RETURN:
            return extended ? 0x10C /* synthetic numpad enter */ : VK_RETURN;
        default:
            return vk;
    }
}
#endif

std::vector<std::string> KeyMap::NamesForVirtualKey(uint32_t vk, uint32_t /*scan*/, bool extended)
{
#ifdef _WIN32
    if(vk == VK_RETURN && extended)
    {
        return {"Key: Number Pad Enter"};
    }
    vk = NormalizeVk(vk, 0, extended);
#endif
    for(const KeyAlias& a : Aliases())
    {
        if(a.vk == vk)
        {
            return a.names;
        }
    }
    return {};
}

bool KeyMap::NameMatches(const std::string& led_name, const std::vector<std::string>& aliases)
{
    const std::string led = ToLower(led_name);
    for(const std::string& alias : aliases)
    {
        const std::string a = ToLower(alias);
        if(led == a)
        {
            return true;
        }
        if(led.size() > a.size() && led.compare(led.size() - a.size(), a.size(), a) == 0)
        {
            return true;
        }
        if(a.rfind("key: ", 0) == 0)
        {
            const std::string short_name = a.substr(5);
            if(led == short_name)
            {
                return true;
            }
        }
    }
    return false;
}
