#pragma once

// Dialog codec — portable, no UI.
// Server -> client RPC_DialogBox(61):
//   u16 id + u8 style + u8 titleLen + title + u8 btn1len + btn1 +
//   u8 btn2len + btn2 + compressed-string info (StringCompressor).
//   Style 255 = close any active dialog.
// Client -> server RPC_DialogResponse(62):
//   u16 id + u8 button (1 OK/left, 0 cancel/right) + u16 listItem +
//   u8 len + CP1251 bytes (max 128).
// Mirrors OpenSAMP ScrDialogBox/sendDialogResponse (ref a05717e).
// All strings CP1251 on the wire, UTF-8 in memory.

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include "BitStream.h"
#include "StringCompressor.h"
#include "port/util/encoding.h"

namespace opensamp::net
{
struct DialogData {
    std::uint16_t id = 0;
    std::uint8_t style = 0;
    std::string title_utf8;
    std::string button1_utf8;
    std::string button2_utf8;
    std::string info_utf8;
    bool close = false;
};

inline bool decodeDialogBox(const unsigned char* data, unsigned bitLen,
                            DialogData& out)
{
    if (!data || bitLen < 16) return false;
    RakNet::BitStream bs(const_cast<unsigned char*>(data), bitLen / 8 + 1,
                         false);
    DialogData d;
    unsigned char tl = 0, b1l = 0, b2l = 0;
    if (!bs.Read(d.id)) return false;
    if (!bs.Read(d.style)) return false;
    if (d.style == 255) {
        out = DialogData{};
        out.close = true;
        return true;
    }
    char tmp[512];
    if (!bs.Read(tl)) return false;
    if (tl > 0) {
        if (tl >= sizeof(tmp)) return false;
        if (!bs.Read(tmp, tl)) return false;
        d.title_utf8 = util::cp1251_to_utf8({tmp, tl});
    }
    if (!bs.Read(b1l)) return false;
    if (b1l > 0) {
        if (b1l >= sizeof(tmp)) return false;
        if (!bs.Read(tmp, b1l)) return false;
        d.button1_utf8 = util::cp1251_to_utf8({tmp, b1l});
    }
    if (!bs.Read(b2l)) return false;
    if (b2l > 0) {
        if (b2l >= sizeof(tmp)) return false;
        if (!bs.Read(tmp, b2l)) return false;
        d.button2_utf8 = util::cp1251_to_utf8({tmp, b2l});
    }
    char info[4096] = {};
    if (!::StringCompressor::Instance()->DecodeString(info,
                                                            sizeof(info) - 1,
                                                            &bs))
        return false;
    d.info_utf8 = util::cp1251_to_utf8(info);
    out = std::move(d);
    return true;
}

inline bool encodeDialogResponse(std::uint16_t id, std::uint8_t button,
                                 std::uint16_t listItem, std::string_view utf8,
                                 RakNet::BitStream& out)
{
    const std::string payload = util::utf8_to_cp1251(utf8);
    if (payload.size() > 128) return false;
    const auto n = static_cast<unsigned char>(payload.size());
    out.Write(id);
    out.Write(button);
    out.Write(listItem);
    out.Write(n);
    if (n > 0) out.Write(payload.data(), n);
    return true;
}
} // namespace opensamp::net
