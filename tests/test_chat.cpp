// Unit test: chat wire codec with synthetic payloads (no server).
#include <cstdio>
#include <string>

#include "BitStream.h"
#include "port/net/chat.h"

namespace {
int fails = 0;
void check(bool ok, const char* what) {
    if (!ok) { ++fails; std::printf("FAIL %s\n", what); }
}
} // namespace

int main() {
    using opensamp::net::ChatLine;
    using opensamp::net::decodeClientMessage;
    using opensamp::net::encodeChatSend;

    // 1. Decode: color + "Hi {FF0000}red" (CP1251 == ASCII here).
    {
        RakNet::BitStream bs;
        const std::uint32_t color = 0xFF00FF00u;
        const std::string msg = "Hi {FF0000}red";
        bs.Write(color);
        bs.Write(static_cast<std::uint32_t>(msg.size()));
        bs.Write(msg.data(), static_cast<unsigned>(msg.size()));
        ChatLine line;
        check(decodeClientMessage(bs.GetData(), bs.GetNumberOfBitsUsed(), line),
              "decode-ok");
        check(line.color == color, "decode-color");
        check(line.text_utf8 == msg, "decode-text-tags-verbatim");
    }

    // 2. Decode: Cyrillic CP1251 -> UTF-8 ("Privet" -> "Привет" bytes).
    {
        RakNet::BitStream bs;
        const std::string cp1251("\xCF\xF0\xE8\xE2\xE5\xF2", 6);
        bs.Write(static_cast<std::uint32_t>(0xFFFFFFFFu));
        bs.Write(static_cast<std::uint32_t>(6));
        bs.Write(cp1251.data(), 6);
        ChatLine line;
        check(decodeClientMessage(bs.GetData(), bs.GetNumberOfBitsUsed(), line),
              "decode-cyr-ok");
        check(line.text_utf8.size() == 12, "decode-cyr-len"); // 6 x 2-byte UTF-8
    }

    // 3. Decode rejects: overlong length, truncated payload, empty input.
    {
        RakNet::BitStream bs;
        bs.Write(static_cast<std::uint32_t>(0));
        bs.Write(static_cast<std::uint32_t>(300));
        ChatLine line;
        check(!decodeClientMessage(bs.GetData(), bs.GetNumberOfBitsUsed(), line),
              "reject-overlong");
    }
    {
        ChatLine line;
        check(!decodeClientMessage(nullptr, 0, line), "reject-null");
        unsigned char tiny[2] = {0x01, 0x02};
        check(!decodeClientMessage(tiny, 16, line), "reject-short");
    }

    // 4. Encode: UTF-8 Cyrillic -> CP1251 payload shape.
    {
        RakNet::BitStream bs;
        check(encodeChatSend("Privet", bs), "encode-ok");
        check(bs.GetNumberOfBytesUsed() == 1 + 6, "encode-len");
        unsigned char n = 0;
        RakNet::BitStream rd(bs.GetData(), bs.GetNumberOfBytesUsed(), false);
        check(rd.Read(n) && n == 6, "encode-lenbyte");
        char buf[8] = {};
        check(rd.Read(buf, 6) && std::string(buf, 6) == "Privet", "encode-ascii");
    }
    {
        // "Привет" UTF-8 (12B) -> CP1251 (6B): 1 len byte + 6.
        RakNet::BitStream bs;
        const std::string privet_u8 = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
        check(encodeChatSend(privet_u8, bs), "encode-cyr-ok");
        check(bs.GetNumberOfBytesUsed() == 1 + 6, "encode-cyr-len");
    }

    if (fails == 0) std::puts("ALL PASS");
    return fails == 0 ? 0 : 1;
}
