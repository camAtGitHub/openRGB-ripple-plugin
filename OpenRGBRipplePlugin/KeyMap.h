/*---------------------------------------------------------*\
| KeyMap.h                                                  |
|   Windows VK / scan → OpenRGB "Key: …" LED names          |
\*---------------------------------------------------------*/

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

struct KeyAlias
{
    uint32_t                 vk;
    std::vector<std::string> names;
};

class KeyMap
{
public:
    static const std::vector<KeyAlias>& Aliases();

    static std::vector<std::string> NamesForVirtualKey(uint32_t vk, uint32_t scan = 0, bool extended = false);

    static bool NameMatches(const std::string& led_name, const std::vector<std::string>& aliases);

#ifdef _WIN32
    static uint32_t NormalizeVk(uint32_t vk, uint32_t scan, bool extended);
#endif
};
