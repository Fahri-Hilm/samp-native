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
#include <utility>

#include "RakClientInterface.h"
#include "RakNetworkFactory.h"
#include "RakSleep.h"
#include "BitStream.h"
#include "PacketEnumerations.h"
#include "PacketPriority.h"
#include "NetworkTypes.h"
#include "SAMPAuth.h"
#include "port/net/chat.h"
#include "port/net/sync.h"

#include <vector>

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
    enum class State { Idle, Connecting, Accepted, JoinSent, ClassRequested, SpawnRequested };

    // Per-RPC-ID call counts. Static: RakNet wants free functions.
    // Incoming RPCs otherwise vanish inside HandleRPCPacket (see
    // UNDEFINED_RPC_INDEX drop) — that is why an unregistered client
    // "hears nothing" after joining.
    static inline unsigned s_rpcHits[256] = {};
    static inline int s_rpcIds[256] = {};

    template <unsigned Id>
    struct Thunk {
        static void handler(RPCParameters*) { s_rpcHits[Id]++; }
    };

    // PLAYER_SPAWN_INFO wire layout (pack(1), 46 bytes — mirrors upstream).
#pragma pack(push, 1)
    struct SpawnInfo {
        unsigned char team;
        int skin;
        unsigned char unk;
        float pos[3];
        float rotation;
        int weapons[3];
        int weaponAmmo[3];
    };
#pragma pack(pop)
    static_assert(sizeof(SpawnInfo) == 46, "spawn wire layout");

    static inline unsigned char s_spawnOutcome = 0;
    static inline SpawnInfo s_spawn{};
    static inline bool s_spawnReceived = false;
    static inline int s_rpcRequestClass = 128;
    static inline int s_rpcRequestSpawn = 129;
    static inline int s_rpcSpawn = 52;
    static inline int s_rpcChat = 101;
    static inline int s_rpcClientMessage = 93;
    static inline std::vector<ChatLine> s_chatLines;

    static void OnClientMessage(RPCParameters* p)
    {
        s_rpcHits[93]++;
        if (!p || !p->input) return;
        ChatLine line;
        if (decodeClientMessage(p->input, p->numberOfBitsOfData, line))
            s_chatLines.push_back(std::move(line));
    }

    static void OnRequestClass(RPCParameters* p)
    {
        s_rpcHits[128]++;
        if (!p || !p->input) return;
        RakNet::BitStream bs(p->input,
            (p->numberOfBitsOfData / 8) + 1, false);
        unsigned char outcome = 0;
        if (!bs.Read(outcome)) return;
        s_spawnOutcome = outcome;
        if (outcome) {
            SpawnInfo tmp{};
            if (!bs.Read(reinterpret_cast<char*>(&tmp), sizeof(tmp))) return;
            s_spawn = tmp;
            s_spawnReceived = true;
        }
    }

    template <std::size_t... I>
    static void RegisterRange(RakClientInterface* c, std::index_sequence<I...>)
    {
        // 93 and 128 have dedicated decoders, skip the counter thunks.
        ((I != 93 && I != 128 ? c->RegisterAsRemoteProcedureCall(&s_rpcIds[I], &Thunk<I>::handler) : (void)0), ...);
    }

    void RegisterAllRpcs()
    {
        if (!client_) return;
        for (int i = 0; i < 256; ++i) s_rpcIds[i] = i;
        client_->RegisterAsRemoteProcedureCall(&s_rpcRequestClass, &OnRequestClass);
        client_->RegisterAsRemoteProcedureCall(&s_rpcClientMessage, &OnClientMessage);
        RegisterRange(client_, std::make_index_sequence<256>{});
    }

    unsigned rpcTotal() const
    {
        unsigned n = 0;
        for (unsigned h : s_rpcHits) n += h;
        return n;
    }

    HandshakeClient() = default;
    ~HandshakeClient() { Shutdown(); }

    bool Start(const char* host, unsigned short port, const char* nick)
    {
        Shutdown();
        for (unsigned& h : s_rpcHits) h = 0;
        s_spawnOutcome = 0;
        s_spawn = SpawnInfo{};
        s_spawnReceived = false;
        s_chatLines.clear();
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
        RegisterAllRpcs();
        RakNet::BitStream bs;
        bs.Write(cls);
        client_->RPC(&g_RpcRequestClass, &bs, HIGH_PRIORITY, RELIABLE,
                     0, false, UNASSIGNED_NETWORK_ID, nullptr);
        state_ = State::ClassRequested;
    }

    void RequestSpawn()
    {
        if (!client_ || !s_spawnReceived) return;
        RakNet::BitStream bsEmpty;
        client_->RPC(&s_rpcRequestSpawn, &bsEmpty, HIGH_PRIORITY, RELIABLE,
                     0, false, UNASSIGNED_NETWORK_ID, nullptr);
        client_->RPC(&s_rpcSpawn, &bsEmpty, HIGH_PRIORITY, RELIABLE,
                     0, false, UNASSIGNED_NETWORK_ID, nullptr);
        state_ = State::SpawnRequested;
    }

    bool SendChat(const char* utf8)
    {
        if (!client_ || !utf8) return false;
        RakNet::BitStream bs;
        if (!encodeChatSend(utf8, bs)) return false;
        return client_->RPC(&s_rpcChat, &bs, HIGH_PRIORITY, RELIABLE,
                            0, false, UNASSIGNED_NETWORK_ID, nullptr);
    }

    // Raw datagram send (non-RPC: sync packets). Returns bytes queued.
    bool SendOnFoot(const OnFootData& d)
    {
        if (!client_) return false;
        RakNet::BitStream bs;
        encodeOnFootSend(d, bs);
        return client_->Send(&bs, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0);
    }

    bool SendAim(const AimData& a)
    {
        if (!client_) return false;
        RakNet::BitStream bs;
        encodeAimSend(a, bs);
        return client_->Send(&bs, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0);
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
