// Live test: join handshake against a real server (SA-MP 0.3.7 or
// open.mp with allow_037_clients). Server NOT started by this test;
// run one first, e.g. ~/Projects/omp-server-ref/Server/omp-server.
// Env: SAMP_TEST_HOST (default 127.0.0.1), SAMP_TEST_PORT (default 7777).
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "port/net/handshake.h"
#include "RakSleep.h"

namespace {
int fails = 0;
void check(bool ok, const char* what) {
    if (!ok) { ++fails; std::printf("FAIL %s\n", what); }
}
} // namespace

int main() {
    // Live server test: opt-in via SAMP_TEST_LIVE=1 so plain `ctest`
    // stays green without a server. Server must speak 0.3.7
    // (classic samp03svr or open.mp with allow_037_clients).
    const char* live = std::getenv("SAMP_TEST_LIVE");
    if (!live || std::string(live) != "1") {
        std::puts("SKIP (set SAMP_TEST_LIVE=1 with a server running)");
        return 0;
    }
    const char* host = std::getenv("SAMP_TEST_HOST");
    const char* port = std::getenv("SAMP_TEST_PORT");
    const unsigned short p =
        static_cast<unsigned short>(port ? std::atoi(port) : 7777);

    opensamp::net::HandshakeClient c;
    check(c.Start(host ? host : "127.0.0.1", p, "NativeTest"), "start");
    if (c.state() ==
        opensamp::net::HandshakeClient::State::Idle) return 1;

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(20);
    bool classAsked = false;
    while (std::chrono::steady_clock::now() < deadline) {
        c.PollOnce();
        if (c.failed()) break;
        if (c.state() ==
                opensamp::net::HandshakeClient::State::JoinSent &&
            !classAsked) {
            c.RequestClass(0);
            classAsked = true;
        }
        if (classAsked && c.rpcTotal() >= 1
            && c.spawnReceived()) {
            c.RequestSpawn();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    check(!c.failed(), "no-failure");
    check(classAsked, "class-requested");
    // Post-spawn: hold the connection, server should keep talking
    // (world/mark/sync RPCs). Assert the link survives sending Spawn.
    // Also send a chat line and expect the server to echo/broadcast.
    {
        check(c.SendChat("halo dari native [samp-native]"), "chat-sent");
        const auto deadline2 =
            std::chrono::steady_clock::now() + std::chrono::seconds(6);
        while (std::chrono::steady_clock::now() < deadline2) {
            c.PollOnce();
            if (c.failed()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        check(!c.failed(), "spawn-survived");
        check(c.state() ==
            opensamp::net::HandshakeClient::State::SpawnRequested,
            "spawn-state");
        std::printf("  rpcs-after-spawn=%u\n", c.rpcTotal());
        const auto& lines = c.chatLines();
        std::printf("  chat-lines=%zu\n", lines.size());
        check(!lines.empty(), "chat-received-proof");
    }

    // On-foot sync at 20 Hz from our spawn point: the server must keep us
    // alive (AFK timer) and stream us to other players.
    {
        const auto& sp = c.spawn();
        opensamp::net::OnFootData idle =
            opensamp::net::makeIdleOnFoot(sp.pos[0], sp.pos[1], sp.pos[2],
                                          sp.rotation);
        opensamp::net::AimData aim =
            opensamp::net::makeIdleAim(sp.pos[0], sp.pos[1], sp.pos[2]);
        unsigned sent = 0, sentAim = 0;
        const auto deadline3 =
            std::chrono::steady_clock::now() + std::chrono::seconds(10);
        auto next =
            std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() < deadline3) {
            c.PollOnce();
            if (c.failed()) break;
            const auto now = std::chrono::steady_clock::now();
            if (now >= next) {
                if (c.SendOnFoot(idle)) ++sent;
                if (c.SendAim(aim)) ++sentAim;
                next = now + std::chrono::milliseconds(
                    opensamp::net::kOnFootSendRateMs);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
        std::printf("  onfoot-sent=%u aim-sent=%u\n", sent, sentAim);
        check(!c.failed(), "sync-survived");
        check(sent >= 150, "sync-rate"); // 10 s at 20 Hz = 200, slack 150
        check(sentAim >= 150, "aim-rate");

        // Death + respawn: claim wasted, session must survive.
        check(c.SendDeath(0, 0xFFFF), "death-sent");
        {
            const auto deadline4 =
                std::chrono::steady_clock::now() + std::chrono::seconds(6);
            while (std::chrono::steady_clock::now() < deadline4) {
                c.PollOnce();
                if (c.failed()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
            check(!c.failed(), "death-survived");
            std::printf("  rpcs-after-death=%u\n", c.rpcTotal());
        }
    }
    {
        const auto& lines = c.chatLines();
        std::printf("  chat-lines=%zu\n", lines.size());
        for (const auto& ln : lines)
            std::printf("  chat #%08x %s\n", ln.color, ln.text_utf8.c_str());
        check(!lines.empty(), "chat-received");
    }
    check(c.rpcTotal() >= 1, "server-talked-back");
    check(c.spawnReceived(), "spawn-info");
    check(c.spawnOutcome() == 1, "outcome-ok");
    {
        const auto& sp = c.spawn();
        const bool finite = std::isfinite(sp.pos[0]) && std::isfinite(sp.pos[1])
            && std::isfinite(sp.pos[2]) && std::isfinite(sp.rotation);
        std::printf("  skin=%d pos=(%.1f, %.1f, %.1f) rot=%.1f team=%u\n",
            sp.skin, sp.pos[0], sp.pos[1], sp.pos[2], sp.rotation, sp.team);
        check(finite, "spawn-finite");
        check(sp.skin >= 0 && sp.skin <= 311, "spawn-skin-range");
    }
    for (int i = 0; i < 256; ++i)
        if (c.rpcHit(i))
            std::printf("  rpc id=%d count=%u\n", i, c.rpcHit(i));
    c.Shutdown();

    if (fails == 0) std::puts("ALL PASS");
    return fails == 0 ? 0 : 1;
}
