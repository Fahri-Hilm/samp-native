// Tests for port/gui/color_parse.h (vs upstream colored_text.h semantics).
#include <cstdio>
#include <string>

#include "port/gui/color_parse.h"

namespace {
int fails = 0;
void check(bool ok, const char* what) {
    if (!ok) { ++fails; std::printf("FAIL %s\n", what); }
}
using opensamp::gui::color::ParseHexTag;
using opensamp::gui::color::split_lines;
using opensamp::gui::color::tokenize_line;
constexpr std::uint32_t BASE = 0xFFFFFFFFu;
} // namespace

int main() {
    std::uint32_t rgba = 0;
    int len = 0;

    // ParseHexTag: valid forms.
    check(ParseHexTag("{FF0000}", rgba, len) && rgba == 0xFF0000FFu && len == 8, "upper");
    check(ParseHexTag("{00ff88}", rgba, len) && rgba == 0x00FF88FFu && len == 8, "lower");
    check(ParseHexTag("{AbCdEf}", rgba, len) && rgba == 0xABCDEFFFu && len == 8, "mixed");

    // ParseHexTag: rejects. Must not write outputs on failure path shape.
    check(!ParseHexTag("{FFF}", rgba, len), "short");
    check(!ParseHexTag("{GGGGGG}", rgba, len), "nonhex");
    check(!ParseHexTag("{FF0000", rgba, len), "noclose");
    check(!ParseHexTag("{FF0000}X", rgba, len) == false, "tag-plus-tail-ok");
    check(!ParseHexTag("x{FF0000}", rgba, len), "nobrace");
    check(!ParseHexTag(nullptr, rgba, len), "null");
    check(!ParseHexTag("{", rgba, len), "lone");
    check(!ParseHexTag("{FF000}", rgba, len), "seven");

    // tokenize_line basics + stickiness + lone brace.
    {
        std::uint32_t cur = BASE;
        auto segs = tokenize_line("A{FF0000}B", cur);
        check(segs.size() == 2, "seg-count");
        check(segs[0].text == "A" && segs[0].rgba == BASE, "seg0");
        check(segs[1].text == "B" && segs[1].rgba == 0xFF0000FFu, "seg1");
        check(cur == 0xFF0000FFu, "sticky-out");
    }
    {
        std::uint32_t cur = BASE;
        auto segs = tokenize_line("a{b", cur); // lone '{' literal
        check(segs.size() == 3, "lone-count");
        check(segs[0].text == "a", "lone-0");
        check(segs[1].text == "{", "lone-1");
        check(segs[2].text == "b", "lone-2");
        check(cur == BASE, "lone-nocolor");
    }
    {
        std::uint32_t cur = BASE;
        auto segs = tokenize_line("", cur);
        check(segs.empty(), "empty");
    }
    {
        std::uint32_t cur = BASE;
        auto segs = tokenize_line("{00FF00}", cur); // tag only, no text
        check(segs.empty() && cur == 0x00FF00FFu, "tag-only");
    }

    // split_lines: newline handling + cross-line stickiness.
    {
        auto rows = split_lines("hi{FF0000}x\ny", BASE);
        check(rows.size() == 2, "rows");
        check(rows[0].size() == 2 && rows[0][1].rgba == 0xFF0000FFu, "row0");
        check(rows[1].size() == 1 && rows[1][0].text == "y" &&
              rows[1][0].rgba == 0xFF0000FFu, "row1-sticky");
    }
    {
        auto rows = split_lines("a\n", BASE); // trailing newline -> final empty row
        check(rows.size() == 2 && rows[1].empty(), "trailing-nl");
    }
    {
        auto rows = split_lines("", BASE);
        check(rows.size() == 1 && rows[0].empty(), "empty-text");
    }

    if (fails == 0) std::puts("ALL PASS");
    return fails == 0 ? 0 : 1;
}
