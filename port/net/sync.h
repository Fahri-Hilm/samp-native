#pragma once

// On-foot sync — portable, no game.
// Layout mirrors OpenSamp ONFOOT_SYNC_DATA (pack(1), 68 bytes):
// sent as ID_PLAYER_SYNC(207) + raw struct, UNRELIABLE_SEQUENCED,
// every 50 ms (20 Hz — satisfies the server AFK timer).

#include <cmath>
#include <cstdint>
#include <cstring>

#include "BitStream.h"

namespace opensamp::net
{
inline constexpr unsigned char kMsgPlayerSync = 207;
inline constexpr unsigned char kMsgAimSync = 203;
inline constexpr int kOnFootSendRateMs = 50;

#pragma pack(push, 1)
struct OnFootData {
    std::uint16_t lrAnalog = 0;
    std::uint16_t udAnalog = 0;
    std::uint16_t keys = 0;
    float pos[3] = {};
    float quat[4] = {};
    std::uint8_t health = 100;
    std::uint8_t armour = 0;
    std::uint8_t weapon = 0;
    std::uint8_t specialAction = 0;
    float moveSpeed[3] = {};
    float surfOffsets[3] = {};
    std::uint16_t surfInfo = 0;
    int animId = 0;
};
#pragma pack(pop)
static_assert(sizeof(OnFootData) == 68, "on-foot wire size");

// Standing-still facing rotation: yaw-only quaternion (x=y=0).
inline OnFootData makeIdleOnFoot(float x, float y, float z, float rotDeg)
{
    OnFootData d{};
    d.pos[0] = x;
    d.pos[1] = y;
    d.pos[2] = z;
    const float half = rotDeg * 3.14159265f / 360.0f;
    d.quat[0] = 0.0f;
    d.quat[1] = 0.0f;
    d.quat[2] = std::sin(half);
    d.quat[3] = std::cos(half);
    d.health = 100;
    return d;
}

inline bool encodeOnFootSend(const OnFootData& d, RakNet::BitStream& out)
{
    out.Write(kMsgPlayerSync);
    out.Write(reinterpret_cast<const char*>(&d), sizeof(d));
    return true;
}

#pragma pack(push, 1)
struct AimData {
    std::uint8_t camMode = 4;
    float aimF1[3] = {};
    float aimPos[3] = {};
    float aimZ = 0.0f;
    // Wire: ONE byte, low 6 bits = camExtZoom (0-63), high 2 bits = weaponState.
    std::uint8_t extZoomAndWeapon = 0;
    std::uint8_t unk = 0;

    void setZoomWeapon(std::uint8_t zoom6, std::uint8_t weapon2)
    {
        extZoomAndWeapon = static_cast<std::uint8_t>(
            ((weapon2 & 0x3) << 6) | (zoom6 & 0x3F));
    }
};
#pragma pack(pop)
static_assert(sizeof(AimData) == 31, "aim wire size");

inline AimData makeIdleAim(float x, float y, float z)
{
    AimData a{};
    a.aimF1[0] = 0.1f;
    a.aimF1[1] = 0.1f;
    a.aimF1[2] = 0.1f;
    a.aimPos[0] = x;
    a.aimPos[1] = y;
    a.aimPos[2] = z;
    return a;
}

inline bool encodeAimSend(const AimData& a, RakNet::BitStream& out)
{
    out.Write(kMsgAimSync);
    out.Write(reinterpret_cast<const char*>(&a), sizeof(a));
    return true;
}
} // namespace opensamp::net
