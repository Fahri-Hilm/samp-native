// Live test: join handshake against a real server (SA-MP 0.3.7 or
// open.mp with allow_037_clients). Server NOT started by this test;
// run one first, e.g. ~/Projects/omp-server-ref/Server/omp-server.
// Env: SAMP_TEST_HOST (default 127.0.0.1), SAMP_TEST_PORT (default 7777).
#include <chrono>
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
        if (classAsked && c.rpcTotal() >= 1) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    check(!c.failed(), "no-failure");
    check(classAsked, "class-requested");
    std::printf("playerId=%u rpcs=%u\n", c.playerId(), c.rpcTotal());
    check(c.rpcTotal() >= 1, "server-talked-back");
    for (int i = 0; i < 256; ++i)
        if (opensamp::net::HandshakeClient::s_rpcHits[i])
            std::printf("  rpc id=%d count=%u\n", i,
                opensamp::net::HandshakeClient::s_rpcHits[i]);
    c.Shutdown();

    if (fails == 0) std::puts("ALL PASS");
    return fails == 0 ? 0 : 1;
}
