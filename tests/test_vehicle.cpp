// Unit test: vehicle sync codec round-trip (encode -> decode).
#include <cmath>
#include <cstdio>

#include "BitStream.h"
#include "port/net/vehicle.h"

namespace {
int fails = 0;
void check(bool ok, const char* what) {
    if (!ok) { ++fails; std::printf("FAIL %s\n", what); }
}
} // namespace

int main() {
    using opensamp::net::RemoteVehicle;
    using opensamp::net::PassengerSync;
    using opensamp::net::UnoccupiedSync;
    using opensamp::net::MarkerEntry;
    using opensamp::net::decodeVehicleSync;
    using opensamp::net::decodePassengerSync;
    using opensamp::net::decodeUnoccupiedSync;
    using opensamp::net::decodeMarkersSync;
    using opensamp::net::encodeVehicleSend;

    const float q[4] = {0.8660254f, 0.0f, 0.0f, 0.5f};
    const float p[3] = {-1291.7f, 2513.8f, 87.1f};
    const float v[3] = {1.5f, 0.0f, 0.0f};

    // Encode path shape: [200][vid][...] (client -> server, no player id).
    RakNet::BitStream bs;
    check(encodeVehicleSend(411, q, p, v, 1000.0f, bs), "encode-ok");
    check(bs.GetData()[0] == 200, "encode-id");

    // Decode path: server -> client format has a playerId prefix and
    // compressed fields. Build it field by field (mirror of decode).
    RakNet::BitStream bs2;
    bs2.Write(static_cast<unsigned char>(200));
    bs2.Write(static_cast<unsigned short>(7)); // sender player id
    bs2.Write(static_cast<unsigned short>(411)); // vehicle id
    bs2.Write(static_cast<unsigned short>(0)); // lr
    bs2.Write(static_cast<unsigned short>(0)); // ud
    bs2.Write(static_cast<unsigned short>(0)); // keys
    bs2.Write(false);
    bs2.Write(false);
    bs2.Write(false);
    bs2.Write(false); // signs w,x,y,z
    bs2.Write(static_cast<unsigned short>(0));
    bs2.Write(static_cast<unsigned short>(0));
    bs2.Write(static_cast<unsigned short>(32767)); // |z| ~0.5
    bs2.Write(p[0]);
    bs2.Write(p[1]);
    bs2.Write(p[2]);
    bs2.Write(1.5f); // speed magnitude
    bs2.WriteCompressed(1.0f);
    bs2.WriteCompressed(0.0f);
    bs2.WriteCompressed(0.0f);
    bs2.Write(static_cast<unsigned short>(1000)); // car health
    bs2.Write(static_cast<unsigned char>(0xF0)); // hp=100, arm=0
    bs2.Write(static_cast<unsigned char>(0)); // weapon
    bs2.WriteCompressed(false); // siren
    bs2.WriteCompressed(false); // gear
    bs2.WriteCompressed(false); // hydra
    bs2.WriteCompressed(false); // trailer
    bs2.Write(static_cast<std::uint32_t>(0)); // trailer id
    bs2.WriteCompressed(false); // train

    RemoteVehicle rv{};
    check(decodeVehicleSync(bs2.GetData(), bs2.GetNumberOfBitsUsed(), rv),
          "decode-ok");
    check(rv.vehicleId == 411, "vid");
    check(std::fabs(rv.pos[0] + 1291.7f) < 1e-3f, "pos");
    check(std::fabs(rv.quat[0] - 0.8660254f) < 1e-2f, "quat-w");
    check(std::fabs(rv.quat[3] - 0.5f) < 1e-2f, "quat-z");
    check(rv.carHealth == 1000.0f, "carhp");
    check(rv.playerHealth == 100 && rv.playerArmour == 0, "hp-arm");
    check(std::fabs(rv.moveSpeed[0] - 1.5f) < 1e-3f, "vel");

    // Reject: null, truncated, bad ids.
    {
        RemoteVehicle q2{};
        check(!decodeVehicleSync(nullptr, 0, q2), "reject-null");
        unsigned char tiny[3] = {200, 0, 0};
        check(!decodeVehicleSync(tiny, 24, q2), "reject-short");
    }
    {
        RakNet::BitStream b2;
        b2.Write(static_cast<unsigned char>(200));
        b2.Write(static_cast<unsigned short>(5000)); // bad pid
        b2.Write(static_cast<unsigned short>(1));
        RemoteVehicle q2{};
        check(!decodeVehicleSync(b2.GetData(), b2.GetNumberOfBitsUsed(), q2),
              "reject-badpid");
    }

    // Passenger: fixed 24B after pid.
    {
        RakNet::BitStream bp;
        bp.Write(static_cast<unsigned char>(211));
        bp.Write(static_cast<unsigned short>(3));
        bp.Write(static_cast<unsigned short>(401)); // vehicle
        bp.Write(static_cast<unsigned char>(0x85)); // seat 5 + drive-by
        bp.Write(static_cast<unsigned char>(24)); // weapon
        bp.Write(static_cast<unsigned char>(100)); // hp
        bp.Write(static_cast<unsigned char>(50)); // arm
        bp.Write(static_cast<unsigned short>(0));
        bp.Write(static_cast<unsigned short>(0));
        bp.Write(static_cast<unsigned short>(0));
        bp.Write(10.0f);
        bp.Write(20.0f);
        bp.Write(30.0f);
        unsigned short pid = 0;
        PassengerSync ps{};
        check(decodePassengerSync(bp.GetData(), bp.GetNumberOfBitsUsed(), pid,
                                  ps),
              "pass-ok");
        check(pid == 3 && ps.vehicleId == 401, "pass-veh");
        check(ps.seatDriveBy == 0x85 && ps.weapon == 24, "pass-seat");
        check(ps.pos[0] == 10.0f && ps.pos[2] == 30.0f, "pass-pos");
    }

    // Unoccupied: fixed 67B after pid.
    {
        RakNet::BitStream bu;
        bu.Write(static_cast<unsigned char>(209));
        bu.Write(static_cast<unsigned short>(4));
        bu.Write(static_cast<unsigned short>(402));
        for (int i = 0; i < 6; ++i) bu.Write(static_cast<short>(0));
        for (int i = 0; i < 13; ++i)
            bu.Write(static_cast<unsigned char>(0));
        bu.Write(1.0f);
        bu.Write(2.0f);
        bu.Write(3.0f);
        for (int i = 0; i < 3; ++i) bu.Write(0.0f);
        for (int i = 0; i < 3; ++i) bu.Write(0.0f);
        bu.Write(500.0f);
        unsigned short pid = 0;
        UnoccupiedSync us{};
        check(decodeUnoccupiedSync(bu.GetData(), bu.GetNumberOfBitsUsed(), pid,
                                   us),
              "unocc-ok");
        check(pid == 4 && us.vehicleId == 402, "unocc-veh");
        check(us.pos[0] == 1.0f && us.health == 500.0f, "unocc-pos-hp");
    }

    // Markers: count + entries (active with pos, inactive without).
    {
        RakNet::BitStream bm;
        bm.Write(static_cast<unsigned char>(208));
        bm.Write(2);
        bm.Write(static_cast<unsigned short>(1));
        bm.WriteCompressed(true);
        bm.Write(static_cast<short>(-1291));
        bm.Write(static_cast<short>(2513));
        bm.Write(static_cast<short>(87));
        bm.Write(static_cast<unsigned short>(2));
        bm.WriteCompressed(false);
        std::vector<MarkerEntry> ms;
        check(decodeMarkersSync(bm.GetData(), bm.GetNumberOfBitsUsed(), ms),
              "mark-ok");
        check(ms.size() == 2, "mark-count");
        check(ms[0].active && ms[0].pos[0] == -1291.0f, "mark-pos");
        check(!ms[1].active, "mark-inactive");
    }

    if (fails == 0) std::puts("ALL PASS");
    return fails == 0 ? 0 : 1;
}
