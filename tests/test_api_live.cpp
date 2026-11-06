// Live test: full join+chat over the C ABI (what Unity will load).
// Gated: SAMP_TEST_LIVE=1, SAMP_TEST_HOST/PORT.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include "port/net/samp_native_api.h"

namespace {
int fails = 0;
void check(bool ok, const char* what) {
    if (!ok) { ++fails; std::printf("FAIL %s\n", what); }
}
static std::string lastChat;
static void onChat(const char* utf8, unsigned, void*) {
    if (utf8) lastChat = utf8;
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

    SampClient* c = samp_create();
    check(samp_connect(c, host ? host : "127.0.0.1", p, "AbiNative") == 0,
          "connect-start");
    samp_set_chat_cb(c, onChat, nullptr);
    // Drive: poll until spawned (state 5), auto-advance like chatcli.
    const auto dl = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    bool classAsked = false, spawnAsked = false;
    while (std::chrono::steady_clock::now() < dl) {
        samp_poll(c);
        if (samp_failed(c)) break;
        const int st = samp_state(c);
        if (st == 3 && !classAsked) {
            samp_request_class(c, 0);
            classAsked = true;
        }
        if (classAsked && samp_spawn_received(c) && !spawnAsked) {
            samp_request_spawn(c);
            spawnAsked = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    // NOTE: class/spawn requests go through samp_request_* (driven here).
    check(spawnAsked, "spawned");
    if (spawnAsked) {
        check(samp_send_chat(c, "halo via C ABI") == 0, "chat-sent");
        const auto dl2 = std::chrono::steady_clock::now() +
                         std::chrono::seconds(8);
        int pushed = 0;
        while (std::chrono::steady_clock::now() < dl2) {
            samp_poll(c);
            if (samp_failed(c)) break;
            pushed += samp_push_state(c);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        check(!samp_failed(c), "alive");
        check(pushed > 50, "pushed-sync");
        check(!lastChat.empty(), "got-chat");
        std::printf("  pushed=%d lastChat=%.40s\n", pushed,
                    lastChat.c_str());
    }
    samp_destroy(c);
    if (fails == 0) std::puts("ALL PASS");
    return fails == 0 ? 0 : 1;
}
