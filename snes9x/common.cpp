#include "frontend.h"

namespace snes9x_frontend {

vk_u32 min_u32(vk_u32 lhs, vk_u32 rhs)
{
    return lhs < rhs ? lhs : rhs;
}

vk_u32 max_u32(vk_u32 lhs, vk_u32 rhs)
{
    return lhs > rhs ? lhs : rhs;
}

namespace {

char ascii_to_lower(char ch)
{
    return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch;
}

} // namespace

int compare_casefolded(const char* lhs, const char* rhs)
{
    if (lhs == nullptr && rhs == nullptr)
        return 0;
    if (lhs == nullptr)
        return -1;
    if (rhs == nullptr)
        return 1;

    while (*lhs != '\0' || *rhs != '\0') {
        const char lhs_ch = ascii_to_lower(*lhs);
        const char rhs_ch = ascii_to_lower(*rhs);
        if (lhs_ch != rhs_ch)
            return static_cast<unsigned char>(lhs_ch) < static_cast<unsigned char>(rhs_ch) ? -1 : 1;
        if (*lhs != '\0')
            ++lhs;
        if (*rhs != '\0')
            ++rhs;
    }

    return 0;
}

bool ends_with_casefolded(const char* text, const char* suffix)
{
    if (text == nullptr || suffix == nullptr)
        return false;

    const size_t text_length = strlen(text);
    const size_t suffix_length = strlen(suffix);
    if (suffix_length > text_length)
        return false;

    return compare_casefolded(text + (text_length - suffix_length), suffix) == 0;
}

const char* path_basename(const char* path)
{
    if (path == nullptr || path[0] == '\0')
        return "";

    const char* slash = strrchr(path, '/');
    if (slash != nullptr && slash[1] != '\0')
        return slash + 1;
    return path;
}

std::string path_parent(const std::string& path)
{
    const size_t slash = path.rfind('/');
    if (slash == std::string::npos || slash == 0)
        return "/";
    return path.substr(0, slash);
}

std::string path_join(const std::string& parent, const std::string& child)
{
    std::string result;
    if (parent.empty() || parent == "/") {
        result = "/";
    } else {
        result = parent;
        result += "/";
    }
    result += child;
    return result;
}

vk_u32 pack_pixel(unsigned char r, unsigned char g, unsigned char b, vk_pixel_format_t format)
{
    return (static_cast<vk_u32>(r) << 16) | (static_cast<vk_u32>(g) << 8) | static_cast<vk_u32>(b);
}

vk_u32 rgb565_to_pixel(uint16_t pixel, vk_pixel_format_t format)
{
    const vk_u32 red = static_cast<vk_u32>((pixel >> 11) & 0x1Fu);
    const vk_u32 green = static_cast<vk_u32>((pixel >> 5) & 0x3Fu);
    const vk_u32 blue = static_cast<vk_u32>(pixel & 0x1Fu);

    const unsigned char r8 = static_cast<unsigned char>((red * 255u) / 31u);
    const unsigned char g8 = static_cast<unsigned char>((green * 255u) / 63u);
    const unsigned char b8 = static_cast<unsigned char>((blue * 255u) / 31u);
    return pack_pixel(r8, g8, b8, format);
}

} // namespace snes9x_frontend
