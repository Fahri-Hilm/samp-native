// C ABI implementation over HandshakeClient (single global instance).
#include <chrono>
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

int samp_send_chat(SampClient*, const char* utf8) {
    return (utf8 && g_client.SendChat(utf8)) ? 0 : -1;
}

namespace {
float g_lx = 0, g_ly = 0, g_lz = 0, g_lr = 0;
bool g_havePos = false;
std::chrono::steady_clock::time_point g_nextSync{};
} // namespace

void samp_set_local_pos(SampClient*, float x, float y, float z, float rot) {
    g_lx = x;
    g_ly = y;
    g_lz = z;
    g_lr = rot;
    g_havePos = true;
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
