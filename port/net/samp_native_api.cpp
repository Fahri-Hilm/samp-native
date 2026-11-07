// C ABI implementation over HandshakeClient (single global instance).
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "port/net/handshake.h"
#include "port/net/samp_native_api.h"

namespace {
opensamp::net::HandshakeClient g_client;
SampChatCb g_chatCb = nullptr;
void* g_chatCtx = nullptr;
SampRelayCb g_relayCb = nullptr;
void* g_relayCtx = nullptr;
std::size_t g_chatSeen = 0;
std::size_t g_relaySeen = 0;
} // namespace

struct SampClient {};

SampClient* samp_create() { return reinterpret_cast<SampClient*>(1); }
void samp_destroy(SampClient*) {}

void samp_set_chat_cb(SampClient*, SampChatCb cb, void* ctx) {
    g_chatCb = cb;
    g_chatCtx = ctx;
}
void samp_set_relay_cb(SampClient*, SampRelayCb cb, void* ctx) {
    g_relayCb = cb;
    g_relayCtx = ctx;
}
static void pumpChat(SampClient*) {
    const auto& lines = g_client.chatLines();
    while (g_chatSeen < lines.size()) {
        const auto& ln = lines[g_chatSeen++];
        if (g_chatCb) g_chatCb(ln.text_utf8.c_str(), ln.color, g_chatCtx);
    }
    const auto& relays = g_client.relayLines();
    while (g_relaySeen < relays.size()) {
        const auto& rl = relays[g_relaySeen++];
        if (g_relayCb) g_relayCb(rl.first, rl.second.c_str(), g_relayCtx);
    }
}

int samp_connect(SampClient*, const char* host, unsigned short port,
                 const char* nick) {
    g_chatSeen = 0;
    g_relaySeen = 0;
    if (!g_client.Start(host ? host : "127.0.0.1", port,
                        nick ? nick : "Native"))
        return -1;
    return 0;
}

int samp_request_class(SampClient*, int cls) {
    g_client.RequestClass(cls);
    return 0;
}

int samp_request_spawn(SampClient*) {
    g_client.RequestSpawn();
    return 0;
}

void samp_poll(SampClient*) {
    using S = opensamp::net::HandshakeClient::State;
    g_client.PollOnce();
    if (g_client.state() == S::JoinSent && !g_client.spawnReceived()) {
        // Auto-advance join flow for engine callers that only poll.
    }
    pumpChat(nullptr);
}

int samp_failed(SampClient*) { return g_client.failed() ? 1 : 0; }

int samp_state(SampClient*) {
    using S = opensamp::net::HandshakeClient::State;
    switch (g_client.state()) {
        case S::Idle: return 0;
        case S::Connecting: return 1;
        case S::Accepted: return 2;
        case S::JoinSent: return 3;
        case S::ClassRequested: return 4;
        case S::SpawnRequested: return 5;
    }
    return 0;
}

int samp_spawn_received(SampClient*) {
    return g_client.spawnReceived() ? 1 : 0;
}

int samp_send_chat(SampClient*, const char* utf8) {
    return (utf8 && g_client.SendChat(utf8)) ? 0 : -1;
}

namespace {
float g_lx = 0, g_ly = 0, g_lz = 0, g_lr = 0;
bool g_havePos = false;
std::chrono::steady_clock::time_point g_posAt{};
std::chrono::steady_clock::time_point g_nextSync{};
} // namespace

void samp_set_local_pos(SampClient*, float x, float y, float z, float rot) {
    g_lx = x;
    g_ly = y;
    g_lz = z;
    g_lr = rot;
    g_havePos = true;
    g_posAt = std::chrono::steady_clock::now();
}

int samp_push_state(SampClient*) {
    if (!g_client.spawnReceived()) return 0;
    const auto now = std::chrono::steady_clock::now();
    if (now < g_nextSync) return 0;
    g_nextSync = now + std::chrono::milliseconds(
        opensamp::net::kOnFootSendRateMs);
    const auto& sp = g_client.spawn();
    float x = sp.pos[0], y = sp.pos[1], z = sp.pos[2], r = sp.rotation;
    // Test hook: force position near an observer. Format "x y z [rot]".
    if (const char* fp = std::getenv("SAMP_FORCE_POS")) {
        float fx = x, fy = y, fz = z, fr = r;
        if (std::sscanf(fp, "%f %f %f %f", &fx, &fy, &fz, &fr) >= 3) {
            x = fx;
            y = fy;
            z = fz;
            r = fr;
        }
    } else if (g_havePos &&
               now - g_posAt < std::chrono::milliseconds(500)) {
        x = g_lx;
        y = g_ly;
        z = g_lz;
        r = g_lr;
    }
    const auto idle = opensamp::net::makeIdleOnFoot(x, y, z, r);
    const auto aim = opensamp::net::makeIdleAim(x, y, z);
    static int dbgCount = 0;
    if (std::getenv("SAMP_DEBUG_PUSH") && dbgCount < 3) {
        ++dbgCount;
        std::printf("[push] pos=(%.2f, %.2f, %.2f) rot=%.2f hp=%u\n", x, y,
                    z, r, (unsigned)idle.health);
        fflush(stdout);
    }
    const bool ok = g_client.SendOnFoot(idle) && g_client.SendAim(aim);
    return ok ? 1 : 0;
}

int samp_remote_count(SampClient*) {
    return static_cast<int>(g_client.remotes().size());
}

int samp_remote_get(SampClient*, int index, unsigned short* id, float* x,
                    float* y, float* z, float* hp) {
    const auto& m = g_client.remotes();
    if (index < 0 || static_cast<std::size_t>(index) >= m.size()) return -1;
    auto it = m.begin();
    std::advance(it, index);
    if (id) *id = it->first;
    if (x) *x = it->second.pos[0];
    if (y) *y = it->second.pos[1];
    if (z) *z = it->second.pos[2];
    if (hp) *hp = static_cast<float>(it->second.health);
    return 0;
}

int samp_roster_count(SampClient*) {
    return static_cast<int>(g_client.roster().size());
}

int samp_roster_get(SampClient*, int index, unsigned short* id, char* nameOut,
                    int nameCap, int* skin) {
    const auto& m = g_client.roster();
    if (index < 0 || static_cast<std::size_t>(index) >= m.size() || nameCap <= 0)
        return -1;
    auto it = m.begin();
    std::advance(it, index);
    if (id) *id = it->first;
    if (nameOut) {
        std::snprintf(nameOut, static_cast<std::size_t>(nameCap), "%s",
                      it->second.name);
    }
    if (skin) *skin = it->second.skin;
    return 0;
}
