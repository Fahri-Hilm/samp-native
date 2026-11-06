#pragma once

// Portable port of opensamp `native/util/encoding.h`
// (upstream ref a05717e, 2026-08-23).
// Upstream uses <Windows.h> (MultiByteToWideChar / WideCharToMultiByte).
// This port carries a static CP1251 table and hand-rolled UTF-8 codec:
// zero OS dependency. API names kept identical so call sites can switch
// mechanically. Behaviour notes vs Win32:
//  - byte 0x98 (undefined in CP1251) decodes to U+FFFD.
//  - code points with no CP1251 mapping encode to '?'.

#include <cstdint>
#include <string>
#include <string_view>

namespace opensamp::util
{
namespace detail
{
// UCS code point for CP1251 byte 0x80..0xFF. 0xFFFFFFFF = undefined (0x98).
inline constexpr std::uint32_t kCp1251ToUcs[128] = {
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, // 80-87
    0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F, // 88-8F
    0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, // 90-97
    0xFFFFFFFF, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F, // 98-9F
    0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7, // A0-A7
    0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407, // A8-AF
    0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7, // B0-B7
    0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457, // B8-BF
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, // C0-C7
    0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F, // C8-CF
    0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, // D0-D7
    0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F, // D8-DF
    0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, // E0-E7
    0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F, // E8-EF
    0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, // F0-F7
    0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F, // F8-FF
};

inline void append_utf8(std::string& out, std::uint32_t cp)
{
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// Returns next code point, advances i. Invalid sequences -> U+FFFD, skip 1 byte.
inline std::uint32_t next_utf8(std::string_view s, std::size_t& i)
{
    const auto b0 = static_cast<unsigned char>(s[i]);
    if (b0 < 0x80) { ++i; return b0; }
    const std::size_t left = s.size() - i;
    if ((b0 & 0xE0) == 0xC0 && left >= 2) {
        const auto b1 = static_cast<unsigned char>(s[i + 1]);
        if ((b1 & 0xC0) == 0x80) {
            const std::uint32_t cp =
                (static_cast<std::uint32_t>(b0 & 0x1F) << 6) | (b1 & 0x3F);
            if (cp >= 0x80) { i += 2; return cp; }
        }
    } else if ((b0 & 0xF0) == 0xE0 && left >= 3) {
        const auto b1 = static_cast<unsigned char>(s[i + 1]);
        const auto b2 = static_cast<unsigned char>(s[i + 2]);
        if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80) {
            const std::uint32_t cp = (static_cast<std::uint32_t>(b0 & 0x0F) << 12) |
                (static_cast<std::uint32_t>(b1 & 0x3F) << 6) | (b2 & 0x3F);
            if (cp >= 0x800 && !(cp >= 0xD800 && cp <= 0xDFFF)) { i += 3; return cp; }
        }
    } else if ((b0 & 0xF8) == 0xF0 && left >= 4) {
        const auto b1 = static_cast<unsigned char>(s[i + 1]);
        const auto b2 = static_cast<unsigned char>(s[i + 2]);
        const auto b3 = static_cast<unsigned char>(s[i + 3]);
        if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80 && (b3 & 0xC0) == 0x80) {
            const std::uint32_t cp = (static_cast<std::uint32_t>(b0 & 0x07) << 18) |
                (static_cast<std::uint32_t>(b1 & 0x3F) << 12) |
                (static_cast<std::uint32_t>(b2 & 0x3F) << 6) | (b3 & 0x3F);
            if (cp >= 0x10000 && cp <= 0x10FFFF) { i += 4; return cp; }
        }
    }
    ++i;
    return 0xFFFD;
}
} // namespace detail

// Inbound: bytes from the server (CP1251) -> UTF-8.
inline std::string cp1251_to_utf8(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else {
            const std::uint32_t cp = detail::kCp1251ToUcs[c - 0x80];
            detail::append_utf8(out, cp == 0xFFFFFFFF ? 0xFFFD : cp);
        }
    }
    return out;
}

// Outbound: UTF-8 from our UI -> bytes for the server (CP1251).
inline std::string utf8_to_cp1251(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        const std::uint32_t cp = detail::next_utf8(s, i);
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
            continue;
        }
        int found = -1;
        for (int b = 0; b < 128; ++b) {
            if (detail::kCp1251ToUcs[b] == cp) { found = b; break; }
        }
        out.push_back(found < 0 ? '?' : static_cast<char>(0x80 + found));
    }
    return out;
}
} // namespace opensamp::util
