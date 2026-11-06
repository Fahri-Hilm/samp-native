#pragma once

// Incoming on-foot sync decode — portable, no game.
// Mirrors OpenSamp Packet_PlayerSync (upstream ref a05717e):
//   ID_PLAYER_SYNC(207) + PLAYERID + compressed fields:
//   bool hasLR [u16] | bool hasUD [u16] | u16 keys | 3x float pos |
//   norm quat (4 sign bits + 3x u16) | u8 hp/armour nibbles |
//   u8 weapon | u8 special | vector (float mag + 3x compressed float) |
//   bool surf [+ u16 + 3x float] | bool anim [+ int].

#include <cmath>
#include <cstdint>

#include "BitStream.h"

namespace opensamp::net
{
struct RemotePlayer {
    unsigned short id = 0;
    std::uint16_t lrAnalog = 0;
    std::uint16_t udAnalog = 0;
    std::uint16_t keys = 0;
    float pos[3] = {};
    float quat[4] = {1.0f, 0.0f, 0.0f, 0.0f}; // w,x,y,z
    std::uint8_t health = 100;
    std::uint8_t armour = 0;
    std::uint8_t weapon = 0;
    std::uint8_t specialAction = 0;
    float moveSpeed[3] = {};
};

inline bool decodePlayerSync(const unsigned char* data, unsigned bitLen,
                             RemotePlayer& out)
{
    if (!data || bitLen < 8 + 16) return false;
    RakNet::BitStream bs(const_cast<unsigned char*>(data), bitLen / 8 + 1,
                         false);
    bs.IgnoreBits(8); // ID_PLAYER_SYNC
    unsigned short pid = 0;
    if (!bs.Read(pid)) return false;
    if (pid >= 1000) return false;

    RemotePlayer p;
    p.id = pid;

    bool hasLR = false, hasUD = false;
    if (!bs.Read(hasLR)) return false;
    if (hasLR && !bs.Read(p.lrAnalog)) return false;
    if (!bs.Read(hasUD)) return false;
    if (hasUD && !bs.Read(p.udAnalog)) return false;
    if (!bs.Read(p.keys)) return false;
    if (!bs.Read(p.pos[0])) return false;
    if (!bs.Read(p.pos[1])) return false;
    if (!bs.Read(p.pos[2])) return false;
    if (!bs.ReadNormQuat(p.quat[0], p.quat[1], p.quat[2], p.quat[3]))
        return false;

    unsigned char hpArm = 0;
    if (!bs.Read(hpArm)) return false;
    const unsigned arm4 = hpArm & 0x0F, hp4 = (hpArm >> 4) & 0x0F;
    p.armour = (arm4 == 0xF) ? 100 : (arm4 == 0 ? 0 : (std::uint8_t)(arm4 * 7));
    p.health = (hp4 == 0xF) ? 100 : (hp4 == 0 ? 0 : (std::uint8_t)(hp4 * 7));

    if (!bs.Read(p.weapon)) return false;
    if (!bs.Read(p.specialAction)) return false;
    if (!bs.ReadVector(p.moveSpeed[0], p.moveSpeed[1], p.moveSpeed[2]))
        return false;
    // Surf info + animation id are parsed-and-dropped (not needed to
    // render a remote player marker).
    bool hasSurf = false;
    if (!bs.Read(hasSurf)) return false;
    if (hasSurf) {
        std::uint16_t w = 0;
        float f = 0;
        if (!bs.Read(w)) return false;
        if (!bs.Read(f) || !bs.Read(f) || !bs.Read(f)) return false;
    }
    bool hasAnim = false;
    if (!bs.Read(hasAnim)) return false;
    if (hasAnim) {
        int a = 0;
        if (!bs.Read(a)) return false;
    }

    out = p;
    return true;
}

inline bool remoteSane(const RemotePlayer& p)
{
    return std::isfinite(p.pos[0]) && std::isfinite(p.pos[1]) &&
           std::isfinite(p.pos[2]) && p.health <= 100;
}
} // namespace opensamp::net
