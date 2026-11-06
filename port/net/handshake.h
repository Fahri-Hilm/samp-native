#pragma once

// Minimal SAMP 0.3.7 join handshake — portable, no game/gui/Windows.
// Mirrors OpenSamp `Packet_AUTH_KEY` + `Packet_ConnectionSucceeded`
// (upstream ref a05717e) without the game-state writes:
//   RakClient.Connect -> [optional ID_AUTH_KEY] ->
//   ID_CONNECTION_REQUEST_ACCEPTED(playerID, challenge) ->
//   RPC_ClientJoin(25): version, mod, nick, challenge^version, gpci, "0.3.7".
// Server code in the wild (SA-MP 0.3.7, open.mp with allow_037_clients)
// completes this without any game running.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

#include "RakClientInterface.h"
#include "RakNetworkFactory.h"
#include "RakSleep.h"
#include "BitStream.h"
#include "PacketEnumerations.h"
#include "PacketPriority.h"
#include "NetworkTypes.h"
#include "SAMPAuth.h"

namespace opensamp::net
{
inline constexpr int kNetGameVersion = 4057;
inline constexpr const char* kClientVersion = "0.3.7";
inline int g_RpcClientJoin = 25; // RakNet RPC API takes int*
inline int g_RpcRequestClass = 128;

namespace detail
{
inline std::uint32_t bswap32(std::uint32_t v)
{
    return (v >> 24) | ((v >> 8) & 0x0000FF00u) |
           ((v << 8) & 0x00FF0000u) | (v << 24);
}

// Verbatim port of upstream BIG_NUM_MUL_SAFE (SAMP gpci derivation).
inline void BigNumMulSafe(const std::uint8_t inBytes[24],
                          std::uint8_t outBytes[24], std::uint32_t factor)
{
    std::uint32_t inWords[5]{};
    for (int i = 0; i < 5; ++i)
    {
        int o = i * 4;
        inWords[i] = static_cast<std::uint32_t>(inBytes[o]) |
            (static_cast<std::uint32_t>(inBytes[o + 1]) << 8) |
            (static_cast<std::uint32_t>(inBytes[o + 2]) << 16) |
            (static_cast<std::uint32_t>(inBytes[o + 3]) << 24);
    }

    std::uint32_t src[5]{};
    for (int i = 0; i < 5; i++)
        src[i] = bswap32(inWords[4 - i]);

    std::uint32_t outWords[6]{};
    std::uint64_t tmp = 0;

    tmp = static_cast<std::uint64_t>(src[0]) * factor;
    outWords[0] = static_cast<std::uint32_t>(tmp & 0xFFFFFFFFu);
    outWords[1] = static_cast<std::uint32_t>(tmp >> 32);

    for (int k = 1; k < 5; ++k)
    {
        tmp = static_cast<std::uint64_t>(src[k]) * factor + outWords[k];
        outWords[k] = static_cast<std::uint32_t>(tmp & 0xFFFFFFFFu);
        outWords[k + 1] = static_cast<std::uint32_t>(tmp >> 32);
    }

    for (int i = 0; i < 6; ++i)
    {
        int o = i * 4;
        std::uint32_t w = outWords[i];
        outBytes[o] = static_cast<std::uint8_t>(w & 0xFF);
        outBytes[o + 1] = static_cast<std::uint8_t>((w >> 8) & 0xFF);
        outBytes[o + 2] = static_cast<std::uint8_t>((w >> 16) & 0xFF);
        outBytes[o + 3] = static_cast<std::uint8_t>((w >> 24) & 0xFF);
    }

    for (int i = 0; i < 12; i++)
    {
        std::uint8_t t = outBytes[i];
        outBytes[i] = outBytes[23 - i];
        outBytes[23 - i] = t;
    }
}

// Verbatim port of upstream gen_gpci (factor 0x3e9 at join time).
inline int GenGpci(char buf[64], std::uint32_t factor)
{
    static constexpr char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    if (!buf) return 0;
    buf[0] = '0';
    buf[1] = 0;
    if (factor == 0) return 1;

    std::uint8_t raw[24]{};
    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, static_cast<int>(sizeof(alphanum)) - 2);
    for (int i = 0; i < 24; ++i)
        raw[i] = static_cast<std::uint8_t>(alphanum[dist(rng)]);

    std::uint8_t mulOut[24]{};
    BigNumMulSafe(raw, mulOut, factor);

    int pos = 0;
    bool notzero = false;
    for (int i = 0; i < 24; i++)
    {
        std::uint8_t tmp = mulOut[i] >> 4;
        std::uint8_t tmp2 = mulOut[i] & 0x0F;
        if (notzero || tmp)
        {
            buf[pos++] = static_cast<char>((tmp > 9) ? (tmp + 55) : (tmp + 48));
            notzero = true;
        }
        if (notzero || tmp2)
        {
            buf[pos++] = static_cast<char>((tmp2 > 9) ? (tmp2 + 55) : (tmp2 + 48));
            notzero = true;
        }
        if (pos >= 63) break;
    }

    if (!notzero)
    {
        buf[0] = '0';
        buf[1] = 0;
        return 1;
    }
    buf[pos] = 0;
    return pos;
}
} // namespace detail

class HandshakeClient
{
public:
    enum class State { Idle, Connecting, Accepted, JoinSent, ClassRequested };

    HandshakeClient() = default;
    ~HandshakeClient() { Shutdown(); }

    bool Start(const char* host, unsigned short port, const char* nick)
    {
        Shutdown();
        std::snprintf(nick_, sizeof(nick_), "%s", nick ? nick : "Native");
        client_ = RakNetworkFactory::GetRakClientInterface();
        if (!client_) return false;
        if (!client_->Connect(host, port, 0, 0, 5))
        {
            RakNetworkFactory::DestroyRakClientInterface(client_);
            client_ = nullptr;
            return false;
        }
        state_ = State::Connecting;
        return true;
    }

    void PollOnce()
    {
        if (!client_) return;
        Packet* pkt = nullptr;
        while ((pkt = client_->Receive()) != nullptr)
        {
            unsigned char pid = 0;
            if (pkt->data[0] == ID_TIMESTAMP && pkt->length > 5)
                pid = pkt->data[5];
            else if (pkt->length > 0)
                pid = pkt->data[0];

            if (std::getenv("SAMP_DEBUG_PUMP"))
                std::printf("[pump] pid=%u len=%u\n", pid, pkt->length);
            if (pid == ID_AUTH_KEY)
                AnswerAuthKey(pkt);
            else if (pid == ID_CONNECTION_REQUEST_ACCEPTED)
                OnAccepted(pkt);
            else if (pid == ID_RPC)
                ++rpcCount_;
            else if (pid == ID_CONNECTION_ATTEMPT_FAILED ||
                     pid == ID_CONNECTION_BANNED ||
                     pid == ID_INVALID_PASSWORD ||
                     pid == ID_NO_FREE_INCOMING_CONNECTIONS ||
                     pid == ID_DISCONNECTION_NOTIFICATION ||
                     pid == ID_CONNECTION_LOST)
                failed_ = true;

            client_->DeallocatePacket(pkt);
        }
    }

    void RequestClass(int cls = 0)
    {
        if (!client_ || state_ != State::JoinSent) return;
        RakNet::BitStream bs;
        bs.Write(cls);
        client_->RPC(&g_RpcRequestClass, &bs, HIGH_PRIORITY, RELIABLE,
                     0, false, UNASSIGNED_NETWORK_ID, nullptr);
        state_ = State::ClassRequested;
    }

    void Shutdown()
    {
        if (client_)
        {
            client_->Disconnect(300);
            RakNetworkFactory::DestroyRakClientInterface(client_);
            client_ = nullptr;
        }
        state_ = State::Idle;
    }

    State state() const { return state_; }
    unsigned short playerId() const { return playerId_; }
    unsigned rpcCount() const { return rpcCount_; }
    bool failed() const { return failed_; }

private:
    void AnswerAuthKey(Packet* pkt)
    {
        if (pkt->length < 3) return;
        const char* want = reinterpret_cast<const char*>(pkt->data) + 2;
        const char* key = nullptr;
        for (int x = 0; x < 512; ++x)
        {
            if (std::strcmp(want, AuthKeyTable[x][0]) == 0)
            {
                key = AuthKeyTable[x][1];
                break;
            }
        }
        if (!key) return;
        RakNet::BitStream bs;
        bs.Write(static_cast<unsigned char>(ID_AUTH_KEY));
        bs.Write(static_cast<unsigned char>(std::strlen(key)));
        bs.Write(key, static_cast<unsigned>(std::strlen(key)));
        client_->Send(&bs, SYSTEM_PRIORITY, RELIABLE, 0);
    }

    void OnAccepted(Packet* pkt)
    {
        RakNet::BitStream bs(pkt->data, pkt->length, false);
        bs.IgnoreBits(8);   // ID_CONNECTION_REQUEST_ACCEPTED
        bs.IgnoreBits(32);  // binaryAddress
        bs.IgnoreBits(16);  // port
        unsigned short myId = 0;
        unsigned uiChallenge = 0;
        if (!bs.Read(myId)) return;
        if (!bs.Read(uiChallenge)) return;
        playerId_ = myId;
        state_ = State::Accepted;
        SendJoin(uiChallenge);
    }

    void SendJoin(unsigned uiChallenge)
    {
        const unsigned response = uiChallenge ^
            static_cast<unsigned>(kNetGameVersion);
        char gpci[64]{};
        detail::GenGpci(gpci, 0x3e9);
        const unsigned char mod = 1;
        const auto nickLen = static_cast<unsigned char>(std::strlen(nick_));
        const auto gpciLen = static_cast<unsigned char>(std::strlen(gpci));
        const auto verLen =
            static_cast<unsigned char>(std::strlen(kClientVersion));

        RakNet::BitStream bs;
        bs.Write(kNetGameVersion);
        bs.Write(mod);
        bs.Write(nickLen);
        bs.Write(nick_, nickLen);
        bs.Write(response);
        bs.Write(gpciLen);
        bs.Write(gpci, gpciLen);
        bs.Write(verLen);
        bs.Write(kClientVersion, verLen);

        client_->RPC(&g_RpcClientJoin, &bs, HIGH_PRIORITY, RELIABLE,
                     0, false, UNASSIGNED_NETWORK_ID, nullptr);
        state_ = State::JoinSent;
    }

    RakClientInterface* client_ = nullptr;
    State state_ = State::Idle;
    unsigned short playerId_ = 0;
    unsigned rpcCount_ = 0;
    bool failed_ = false;
    char nick_[64]{};
};
} // namespace opensamp::net
