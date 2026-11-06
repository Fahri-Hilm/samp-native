// Two native clients on one server: A must see B's sync and vice versa.
// Live test: needs a server (SAMP_TEST_LIVE=1, SAMP_TEST_HOST/PORT).
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "port/net/handshake.h"

namespace {
int fails = 0;
void check(bool ok, const char* what) {
    if (!ok) { ++fails; std::printf("FAIL %s\n", what); }
}
using opensamp::net::HandshakeClient;
void pumpBoth(HandshakeClient& a, HandshakeClient& b) {
    a.PollOnce();
    b.PollOnce();
}
bool waitFor(bool (*pred)(HandshakeClient&, HandshakeClient&),
             HandshakeClient& a, HandshakeClient& b, int seconds) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        pumpBoth(a, b);
        if (a.failed() || b.failed()) return false;
        if (pred(a, b)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    return false;
}
bool bothJoined(HandshakeClient& a, HandshakeClient& b) {
    return a.state() == HandshakeClient::State::JoinSent &&
           b.state() == HandshakeClient::State::JoinSent;
}
bool bothSpawnInfo(HandshakeClient& a, HandshakeClient& b) {
    return a.spawnReceived() && b.spawnReceived();
}
void dumpRemotes(const char* who, const HandshakeClient& c) {
    for (const auto& [id, rp] : c.remotes())
        std::printf("  %s sees id=%u pos=(%.1f, %.1f, %.1f) hp=%u\n", who,
                    id, rp.pos[0], rp.pos[1], rp.pos[2], rp.health);
}
bool seesOther(const HandshakeClient& me, const HandshakeClient& other) {
    for (const auto& [id, rp] : me.remotes()) {
        if (id == me.playerId()) continue;
        if (!opensamp::net::remoteSane(rp)) continue;
        // Must be the other client: positions match its spawn area.
        const auto& sp = other.spawn();
        const float dx = rp.pos[0] - sp.pos[0];
        const float dy = rp.pos[1] - sp.pos[1];
        if (dx * dx + dy * dy < 2500.0f) return true; // within 50 units
    }
    return false;
}
} // namespace

int main() {
    const char* live = std::getenv("SAMP_TEST_LIVE");
    if (!live || std::string(live) != "1") {
        std::puts("SKIP (set SAMP_TEST_LIVE=1 with a server running)");
        return 0;
    }
    const char* host = std::getenv("SAMP_TEST_HOST");
    const char* port = std::getenv("SAMP_TEST_PORT");
    const unsigned short p =
        static_cast<unsigned short>(port ? std::atoi(port) : 7777);

    HandshakeClient a, b;
    check(a.Start(host ? host : "127.0.0.1", p, "NativeA"), "a-start");
    check(b.Start(host ? host : "127.0.0.1", p, "NativeB"), "b-start");
    check(waitFor(bothJoined, a, b, 20), "both-join");
    a.RequestClass(0);
    b.RequestClass(0);
    check(waitFor(bothSpawnInfo, a, b, 20), "both-spawninfo");
    a.RequestSpawn();
    b.RequestSpawn();

    std::printf("A id=%u spawn=(%.1f, %.1f)\n", a.playerId(), a.spawn().pos[0],
                a.spawn().pos[1]);
    std::printf("B id=%u spawn=(%.1f, %.1f)\n", b.playerId(), b.spawn().pos[0],
                b.spawn().pos[1]);

    // Sync loop: both stand at their spawn points, 20 Hz, ~12 s.
    const auto idleA = opensamp::net::makeIdleOnFoot(
        a.spawn().pos[0], a.spawn().pos[1], a.spawn().pos[2],
        a.spawn().rotation);
    const auto idleB = opensamp::net::makeIdleOnFoot(
        b.spawn().pos[0], b.spawn().pos[1], b.spawn().pos[2],
        b.spawn().rotation);
    const auto aimA = opensamp::net::makeIdleAim(
        a.spawn().pos[0], a.spawn().pos[1], a.spawn().pos[2]);
    const auto aimB = opensamp::net::makeIdleAim(
        b.spawn().pos[0], b.spawn().pos[1], b.spawn().pos[2]);
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(12);
    auto next = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() < deadline) {
        pumpBoth(a, b);
        if (a.failed() || b.failed()) break;
        const auto now = std::chrono::steady_clock::now();
        if (now >= next) {
            a.SendOnFoot(idleA);
            a.SendAim(aimA);
            b.SendOnFoot(idleB);
            b.SendAim(aimB);
            next = now + std::chrono::milliseconds(50);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    check(!a.failed() && !b.failed(), "both-alive");
    std::printf("A syncIn=%u B syncIn=%u\n", a.syncInCount(), b.syncInCount());
    dumpRemotes("A", a);
    dumpRemotes("B", b);
    check(seesOther(a, b), "a-sees-b");
    check(seesOther(b, a), "b-sees-a");

    a.Shutdown();
    b.Shutdown();
    if (fails == 0) std::puts("ALL PASS");
    return fails == 0 ? 0 : 1;
}
