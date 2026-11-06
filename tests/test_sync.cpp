// Unit test: on-foot sync wire shape (no server).
#include <cmath>
#include <cstdio>
#include <cstring>

#include "BitStream.h"
#include "port/net/sync.h"

namespace {
int fails = 0;
void check(bool ok, const char* what) {
    if (!ok) { ++fails; std::printf("FAIL %s\n", what); }
}
} // namespace

int main() {
    using opensamp::net::OnFootData;
    using opensamp::net::AimData;
    using opensamp::net::encodeOnFootSend;
    using opensamp::net::encodeAimSend;
    using opensamp::net::makeIdleAim;
    using opensamp::net::makeIdleOnFoot;

    check(sizeof(OnFootData) == 68, "wire-size");

    // Shape: 1 id byte + 68 raw bytes.
    {
        OnFootData d = makeIdleOnFoot(1.0f, 2.0f, 3.0f, 90.0f);
        RakNet::BitStream bs;
        check(encodeOnFootSend(d, bs), "encode-ok");
        check(bs.GetNumberOfBytesUsed() == 1 + 68, "encode-len");
        check(bs.GetData()[0] == 207, "encode-id");
        // Payload round-trips byte-exact.
        OnFootData back{};
        std::memcpy(&back, bs.GetData() + 1, sizeof(back));
        check(back.pos[0] == 1.0f && back.pos[2] == 3.0f, "encode-pos");
        check(back.health == 100, "encode-hp");
    }

    // Yaw quaternion: 90 deg -> (0,0,sin45,cos45).
    {
        OnFootData d = makeIdleOnFoot(0, 0, 0, 90.0f);
        const float s = std::sin(3.14159265f / 4.0f);
        const float c = std::cos(3.14159265f / 4.0f);
        check(std::fabs(d.quat[2] - s) < 1e-6f, "yaw-sin");
        check(std::fabs(d.quat[3] - c) < 1e-6f, "yaw-cos");
        check(d.quat[0] == 0.0f && d.quat[1] == 0.0f, "yaw-xy");
    }

    // Aim: 31 bytes, id 203, zoom/weapon share one wire byte.
    {
        check(sizeof(AimData) == 31, "aim-size");
        AimData a = makeIdleAim(1.0f, 2.0f, 3.0f);
        a.setZoomWeapon(63, 2);
        check(a.extZoomAndWeapon == 0xBF, "aim-zoomweapon-pack");
        RakNet::BitStream bs;
        check(encodeAimSend(a, bs), "aim-encode-ok");
        check(bs.GetNumberOfBytesUsed() == 1 + 31, "aim-encode-len");
        check(bs.GetData()[0] == 203, "aim-encode-id");
    }

    if (fails == 0) std::puts("ALL PASS");
    return fails == 0 ? 0 : 1;
}
