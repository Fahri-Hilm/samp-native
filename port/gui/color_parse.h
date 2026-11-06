#pragma once

// Portable port of the pure parsing half of opensamp
// `native/gui/colored_text.h` (upstream ref a05717e).
// Rendering (ImGui) stays out: this header tokenizes SAMP `{RRGGBB}`
// text into segments any renderer (terminal, SDL, future engine)
// can consume. Logic mirrors DrawLine/DrawColoredText, including:
//  - `{RRGGBB}` sticks until overridden, across lines;
//  - lone '{' renders as literal text;
//  - empty line yields zero segments (renderer emits a blank row).

#include <cstdint>
#include <string_view>
#include <vector>

namespace opensamp::gui::color
{
// One chunk of plain text in a single color (0xRRGGBBAA, alpha always 0xFF
// from tags; base color supplied by caller).
struct Segment {
    std::string_view text;
    std::uint32_t rgba;
};

// Parse a `{RRGGBB}` tag at `s` (8 chars: `{` + 6 hex + `}`).
// Verbatim port of upstream ParseHexTag.
inline bool ParseHexTag(const char* s, std::uint32_t& outRgba, int& outLen)
{
    if (!s || s[0] != '{') return false;
    for (int i = 1; i <= 6; ++i)
        if (!s[i]) return false;
    if (s[7] != '}') return false;

    const auto hex = [](char c) -> int
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };

    const int n[6] = { hex(s[1]), hex(s[2]), hex(s[3]),
                       hex(s[4]), hex(s[5]), hex(s[6]) };
    for (int v : n) if (v < 0) return false;

    const auto r = static_cast<std::uint8_t>((n[0] << 4) | n[1]);
    const auto g = static_cast<std::uint8_t>((n[2] << 4) | n[3]);
    const auto b = static_cast<std::uint8_t>((n[4] << 4) | n[5]);
    outRgba = (static_cast<std::uint32_t>(r) << 24) |
              (static_cast<std::uint32_t>(g) << 16) |
              (static_cast<std::uint32_t>(b) << 8)  |
              0xFFu;
    outLen = 8;
    return true;
}

// Tokenize one line (no '\n'). `cur` is in/out: caller's current color,
// updated as tags are consumed (stickiness).
inline std::vector<Segment> tokenize_line(std::string_view line, std::uint32_t& cur)
{
    std::vector<Segment> segs;
    const char* p   = line.data();
    const char* end = p + line.size();

    while (p < end)
    {
        std::uint32_t rgba   = 0;
        int           tagLen = 0;
        // Length guard: ParseHexTag reads s[0..7]; never read past `end`.
        // (Upstream relies on NUL termination; views here may not have it.)
        if (*p == '{' && (end - p) >= 8 && ParseHexTag(p, rgba, tagLen))
        {
            cur = rgba;
            p += tagLen;
            continue;
        }

        const char* nextBrace = p;
        while (nextBrace < end && *nextBrace != '{') ++nextBrace;

        if (nextBrace == p && *p == '{')
        {
            segs.push_back({std::string_view(p, 1), cur});
            ++p;
            continue;
        }

        if (nextBrace > p)
        {
            segs.push_back({std::string_view(p,
                static_cast<std::size_t>(nextBrace - p)), cur});
            p = nextBrace;
        }
    }
    return segs;
}

// Split full text on '\n' exactly like upstream DrawColoredText:
// color sticks across lines; trailing '\n' yields a final empty row.
inline std::vector<std::vector<Segment>> split_lines(
    std::string_view text, std::uint32_t base)
{
    std::vector<std::vector<Segment>> rows;
    std::uint32_t cur = base;
    const char* p   = text.data();
    const char* end = p + text.size();

    while (p <= end)
    {
        const char* lineEnd = p;
        while (lineEnd < end && *lineEnd != '\n') ++lineEnd;
        rows.push_back(tokenize_line(
            std::string_view(p, static_cast<std::size_t>(lineEnd - p)), cur));
        if (lineEnd < end && *lineEnd == '\n')
            p = lineEnd + 1;
        else
            break;
    }
    return rows;
}
} // namespace opensamp::gui::color
