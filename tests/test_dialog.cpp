// Unit test: dialog codec with synthetic packets (no server).
#include <cstdio>
#include <cstring>
#include <string>

#include "BitStream.h"
#include "StringCompressor.h"
#include "port/net/dialog.h"

namespace {
int fails = 0;
void check(bool ok, const char* what) {
    if (!ok) { ++fails; std::printf("FAIL %s\n", what); }
}
} // namespace

static void writeStr(RakNet::BitStream& bs, const char* s) {
    const auto n = static_cast<unsigned char>(std::strlen(s));
    bs.Write(n);
    if (n) bs.Write(s, n);
}

int main() {
    ::StringCompressor::AddReference();
    using opensamp::net::DialogData;
    using opensamp::net::decodeDialogBox;
    using opensamp::net::encodeDialogResponse;

    // 1. Full dialog round trip (CP1251 title to exercise transcoding).
    {
        RakNet::BitStream bs;
        bs.Write(static_cast<unsigned short>(7)); // id
        bs.Write(static_cast<unsigned char>(1)); // style: msgbox
        const std::string title_cp("\xCF\xF0\xE8\xE2\xE5\xF2", 6); // "Privet" in CP1251
        bs.Write(static_cast<unsigned char>(title_cp.size()));
        bs.Write(title_cp.data(), static_cast<unsigned>(title_cp.size()));
        writeStr(bs, "OK");
        writeStr(bs, "Cancel");
        ::StringCompressor::Instance()->EncodeString(
            "Welcome {FF0000}red", 4096, &bs);
        DialogData d{};
        check(decodeDialogBox(bs.GetData(), bs.GetNumberOfBitsUsed(), d),
              "decode-ok");
        check(d.id == 7 && d.style == 1, "decode-head");
        check(d.title_utf8 == "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82",
              "decode-title-cp1251");
        check(d.button1_utf8 == "OK", "decode-b1");
        check(d.button2_utf8 == "Cancel", "decode-b2");
        check(d.info_utf8 == "Welcome {FF0000}red", "decode-info-tags");
        check(!d.close, "not-close");
    }

    // 2. Style 255 = close.
    {
        RakNet::BitStream bs;
        bs.Write(static_cast<unsigned short>(0));
        bs.Write(static_cast<unsigned char>(255));
        DialogData d{};
        check(decodeDialogBox(bs.GetData(), bs.GetNumberOfBitsUsed(), d),
              "close-ok");
        check(d.close, "close-flag");
    }

    // 3. Encode response shape.
    {
        RakNet::BitStream bs;
        check(encodeDialogResponse(7, 1, 0xFFFF, "yes", bs), "enc-ok");
        check(bs.GetNumberOfBytesUsed() == 2 + 1 + 2 + 1 + 3, "enc-len");
        RakNet::BitStream rd(bs.GetData(), bs.GetNumberOfBytesUsed(), false);
        unsigned short id = 0;
        unsigned char btn = 0, n = 0;
        unsigned short item = 0;
        check(rd.Read(id) && id == 7, "enc-id");
        check(rd.Read(btn) && btn == 1, "enc-btn");
        check(rd.Read(item) && item == 0xFFFF, "enc-item");
        check(rd.Read(n) && n == 3, "enc-lenbyte");
    }

    // 4. Rejects.
    {
        DialogData d{};
        check(!decodeDialogBox(nullptr, 0, d), "reject-null");
        unsigned char tiny[2] = {61, 0};
        check(!decodeDialogBox(tiny, 16, d), "reject-short");
    }

    if (fails == 0) std::puts("ALL PASS");
    return fails == 0 ? 0 : 1;
}
