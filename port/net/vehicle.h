#pragma once

// Vehicle sync codec — portable, no game.
// Mirrors OpenSamp inCarUpdate (send) + Packet_VehicleSync (decode).
// Wire: ID_VEHICLE_SYNC(200) + VEHICLEID + compressed fields:
// u16 lr/ud/keys | norm quat | 3x float pos | vector speed | u16 carhealth |
// u8 hp/arm nibbles | u8 weapon | bit siren | bit gear | bit hydra |
// bit trailer | u32 trailerId/thrust | bit train [+ u16 trainspeed].

#include <cmath>
#include <cstdint>

#include "BitStream.h"

namespace opensamp::net
{
inline constexpr unsigned char kMsgVehicleSync = 200;

struct RemoteVehicle {
    unsigned short playerId = 0;
    unsigned short vehicleId = 0;
    float quat[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float pos[3] = {};
    float moveSpeed[3] = {};
    float carHealth = 1000.0f;
    std::uint8_t playerHealth = 100;
    std::uint8_t playerArmour = 0;
};

inline bool decodeVehicleSync(const unsigned char* data, unsigned bitLen,
                              RemoteVehicle& out)
{
    if (!data || bitLen < 8 + 16 + 16) return false;
    RakNet::BitStream bs(const_cast<unsigned char*>(data), bitLen / 8 + 1,
                         false);
    bs.IgnoreBits(8); // ID_VEHICLE_SYNC
    unsigned short pid = 0, vid = 0;
    if (!bs.Read(pid)) return false;
    if (!bs.Read(vid)) return false;
    if (pid >= 1000 || vid >= 2000) return false;

    RemoteVehicle v;
    v.playerId = pid;
    v.vehicleId = vid;
    std::uint16_t u16 = 0;
    if (!bs.Read(u16)) return false; // lr
    if (!bs.Read(u16)) return false; // ud
    if (!bs.Read(u16)) return false; // keys
    if (!bs.ReadNormQuat(v.quat[0], v.quat[1], v.quat[2], v.quat[3]))
        return false;
    if (!bs.Read(v.pos[0]) || !bs.Read(v.pos[1]) || !bs.Read(v.pos[2]))
        return false;
    if (!bs.ReadVector(v.moveSpeed[0], v.moveSpeed[1], v.moveSpeed[2]))
        return false;
    std::uint16_t carHp = 0;
    if (!bs.Read(carHp)) return false;
    v.carHealth = static_cast<float>(carHp);
    unsigned char hpArm = 0, weapon = 0;
    if (!bs.Read(hpArm)) return false;
    const unsigned arm4 = hpArm & 0x0F, hp4 = (hpArm >> 4) & 0x0F;
    v.playerArmour = (arm4 == 0xF) ? 100 : (arm4 == 0 ? 0 : (std::uint8_t)(arm4 * 7));
    v.playerHealth = (hp4 == 0xF) ? 100 : (hp4 == 0 ? 0 : (std::uint8_t)(hp4 * 7));
    if (!bs.Read(weapon)) return false;
    bool b = false;
    if (!bs.ReadCompressed(b)) return false; // siren
    if (!bs.ReadCompressed(b)) return false; // gear
    if (!bs.ReadCompressed(b)) return false; // hydra
    if (!bs.ReadCompressed(b)) return false; // trailer
    std::uint32_t extra = 0;
    if (!bs.Read(extra)) return false; // trailer id / thrust
    if (!bs.ReadCompressed(b)) return false; // train
    if (b) {
        std::uint16_t ts = 0;
        if (!bs.Read(ts)) return false;
    }
    out = v;
    return true;
}

// Encode a driver update (mirror of upstream inCarUpdate, fixed values
// where we have no game state yet).
inline bool encodeVehicleSend(unsigned short vehicleId, const float quat[4],
                              const float pos[3], const float vel[3],
                              float carHealth, RakNet::BitStream& out)
{
    out.Write(kMsgVehicleSync);
    out.Write(vehicleId);
    out.Write(static_cast<std::uint16_t>(0)); // lr
    out.Write(static_cast<std::uint16_t>(0)); // ud
    out.Write(static_cast<std::uint16_t>(0)); // keys
    out.WriteNormQuat(quat[0], quat[1], quat[2], quat[3]);
    out.Write(pos[0]);
    out.Write(pos[1]);
    out.Write(pos[2]);
    out.WriteVector(vel[0], vel[1], vel[2]);
    out.Write(static_cast<std::uint16_t>(carHealth));
    out.Write(static_cast<unsigned char>(0xF0)); // hp=100, arm=0
    out.Write(static_cast<unsigned char>(0)); // weapon (drive-by only)
    out.WriteCompressed(false); // siren
    out.WriteCompressed(false); // gear
    out.WriteCompressed(false); // hydra
    out.WriteCompressed(false); // trailer
    out.Write(static_cast<std::uint32_t>(0)); // trailer id
    out.WriteCompressed(false); // train
    return true;
}
} // namespace opensamp::net
