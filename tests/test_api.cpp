// Unit test: C ABI surface without network.
#include <cstdio>
#include <cstring>

#include "port/net/samp_native_api.h"

namespace {
int fails = 0;
void check(bool ok, const char* what) {
    if (!ok) { ++fails; std::printf("FAIL %s\n", what); }
}
static int chatCalls = 0;
static void onChat(const char*, unsigned, void* ctx) {
    ++chatCalls;
    if (ctx) *static_cast<int*>(ctx) = chatCalls;
}
} // namespace

int main() {
    SampClient* c = samp_create();
    check(c != nullptr, "create");
    check(samp_state(c) == 0, "idle");
    check(!samp_failed(c), "not-failed");
    check(samp_send_chat(c, "hi") != 0, "send-chat-fails-offline");
    check(samp_push_state(c) == 0, "push-no-spawn");
    int mark = 0;
    samp_set_chat_cb(c, onChat, &mark);
    samp_poll(c); // null client: must not crash
    check(mark == 0, "no-chat-yet");
    check(samp_remote_count(c) == 0, "no-remotes");
    check(samp_roster_count(c) == 0, "no-roster");
    unsigned short id = 0;
    check(samp_remote_get(c, 0, &id, nullptr, nullptr, nullptr, nullptr) != 0,
          "remote-oob");
    samp_set_local_pos(c, 1.0f, 2.0f, 3.0f, 90.0f);
    samp_poll(c);
    samp_destroy(c);
    check(true, "destroy-ok");
    if (fails == 0) std::puts("ALL PASS");
    return fails == 0 ? 0 : 1;
}
