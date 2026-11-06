// Unit test: incoming player-sync decode with a synthetic packet.
#include <cmath>
#include <cstdio>

#include "BitStream.h"
#include "port/net/sync_in.h"

namespace {
int fails = 0;
void check(bool ok, const char* what) {
    if (!ok) { ++fails; std::printf("FAIL %s\n", what); }
}
} // namespace

int main() {
    using opensamp::net::RemotePlayer;
    using opensamp::net::decodePlayerSync;
    using opensamp::net::remoteSane;

    // Build a packet in the exact read order of decodePlayerSync.
    RakNet::BitStream bs;
    bs.Write(static_cast<unsigned char>(207)); // ID_PLAYER_SYNC
    bs.Write(static_cast<unsigned short>(7)); // player id
    bs.Write(true); // hasLR
    bs.Write(static_cast<unsigned short>(0x1234)); // lr
    bs.Write(false); // hasUD
    bs.Write(static_cast<unsigned short>(0x00FF)); // keys
    bs.Write(-1291.7f);
    bs.Write(2513.8f);
    bs.Write(87.1f);
    // NormQuat(w,x,y,z) -> 4 sign bits + 3 u16. x=y=0, z=0.5.
    bs.Write(false);
    bs.Write(false);
    bs.Write(false);
    bs.Write(false);
    bs.Write(static_cast<unsigned short>(0));
    bs.Write(static_cast<unsigned short>(0));
    bs.Write(static_cast<unsigned short>(32768));
    bs.Write(static_cast<unsigned char>(0xF0)); // hp=100, arm=0
    bs.Write(static_cast<unsigned char>(24)); // weapon: desert eagle
    bs.Write(static_cast<unsigned char>(0)); // special
    // moveSpeed: magnitude + 3 compressed floats in [-1,1].
    bs.Write(2.0f);
    bs.WriteCompressed(0.6f);
    bs.WriteCompressed(0.0f);
    bs.WriteCompressed(0.8f);
    bs.Write(false); // no surf
    bs.Write(true); // anim
    bs.Write(1185);

    RemotePlayer p{};
    check(decodePlayerSync(bs.GetData(), bs.GetNumberOfBitsUsed(), p),
          "decode-ok");
    check(p.id == 7, "id");
    check(p.lrAnalog == 0x1234 && p.udAnalog == 0, "analogs");
    check(p.keys == 0x00FF, "keys");
    check(std::fabs(p.pos[0] + 1291.7f) < 1e-3f, "pos-x");
    check(std::fabs(p.quat[3] - 0.5f) < 1e-3f, "quat-z");
    check(std::fabs(p.quat[0] - 0.8660254f) < 1e-3f, "quat-w");
    check(p.health == 100 && p.armour == 0, "hp-arm");
    check(p.weapon == 24 && p.specialAction == 0, "weap-spec");
    check(std::fabs(p.moveSpeed[0] - 1.2f) < 1e-3f, "spd-x");
    check(std::fabs(p.moveSpeed[2] - 1.6f) < 1e-3f, "spd-z");
    check(remoteSane(p), "sane");

    // Reject: null, truncated, bad id.
    {
        RemotePlayer q{};
        check(!decodePlayerSync(nullptr, 0, q), "reject-null");
        unsigned char tiny[2] = {207, 0};
        check(!decodePlayerSync(tiny, 16, q), "reject-short");
    }
    {
        RakNet::BitStream b2;
        b2.Write(static_cast<unsigned char>(207));
        b2.Write(static_cast<unsigned short>(5000));
        RemotePlayer q{};
        check(!decodePlayerSync(b2.GetData(), b2.GetNumberOfBitsUsed(), q),
              "reject-badid");
    }

    if (fails == 0) std::puts("ALL PASS");
    return fails == 0 ? 0 : 1;
}
