#pragma once

// Chat wire codec — portable, no game/UI.
// Layouts mirror OpenSamp sendChat/ClientMessage (upstream ref a05717e):
//   C->S RPC_Chat(101): BYTE len + CP1251 bytes.
//   S->C RPC_ClientMessage(93): DWORD color + DWORD len + CP1251 bytes
//   ({RRGGBB} tags pass through verbatim — ASCII, see color_parse.h).

#include <cstdint>
#include <string>
#include <string_view>

#include "BitStream.h"
#include "port/util/encoding.h"

namespace opensamp::net
{
struct ChatLine {
    std::uint32_t color = 0;
    std::string text_utf8;
};

// Decode a server ClientMessage payload into a ChatLine.
// Returns false on malformed input (overlong length, short read).
inline bool decodeClientMessage(const unsigned char* data, unsigned bitLen,
                                ChatLine& out)
{
    if (!data || bitLen < 64) return false;
    RakNet::BitStream bs(const_cast<unsigned char*>(data), bitLen / 8 + 1, false);
    std::uint32_t color = 0, len = 0;
    if (!bs.Read(color)) return false;
    if (!bs.Read(len)) return false;
    if (len > 256) return false;
    if (len == 0) {
        out = ChatLine{color, {}};
        return true;
    }
    std::string raw(len, '\0');
    if (!bs.Read(&raw[0], len)) return false;
    out.color = color;
    out.text_utf8 = util::cp1251_to_utf8(std::string_view(raw.data(), len));
    return true;
}

// Encode our UTF-8 text to a Chat send payload (BYTE len + CP1251).
inline bool encodeChatSend(std::string_view utf8, RakNet::BitStream& out)
{
    const std::string payload = util::utf8_to_cp1251(utf8);
    if (payload.size() > 255) return false;
    const auto n = static_cast<unsigned char>(payload.size());
    out.Write(n);
    if (n > 0) out.Write(payload.data(), n);
    return true;
}

// Incoming player chat relay (server rebroadcast, RPC id 101):
// u16 senderId + u8 len + CP1251 bytes (no color).
inline bool decodeChatRelay(const unsigned char* data, unsigned bitLen,
                            unsigned short& senderOut, std::string& textOut)
{
    if (!data || bitLen < 8 + 16 + 8) return false;
    RakNet::BitStream bs(const_cast<unsigned char*>(data), bitLen / 8 + 1,
                         false);
    // NOTE: no id byte to skip — HandleRPCPacket strips the RPC header;
    // input starts at the sender id (same as upstream Chat()).
    unsigned short sender = 0;
    unsigned char n = 0;
    if (!bs.Read(sender)) return false;
    if (sender >= 1000) return false;
    if (!bs.Read(n)) return false;
    if (n == 0) {
        senderOut = sender;
        textOut.clear();
        return true;
    }
    std::string raw(n, '\0');
    if (!bs.Read(&raw[0], n)) return false;
    senderOut = sender;
    textOut = util::cp1251_to_utf8(std::string_view(raw.data(), n));
    return true;
}
} // namespace opensamp::net
