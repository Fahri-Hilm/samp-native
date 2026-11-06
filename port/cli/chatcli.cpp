// chatcli — terminal SAMP client (native, no game).
// Join + spawn + 20 Hz sync + interactive chat.
// Usage: chatcli <host> <port> <nick>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include <sys/select.h>
#include <unistd.h>

#include "port/net/handshake.h"

namespace {
bool stdinLine(std::string& out) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    timeval tv{0, 5000};
    if (select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv) <= 0)
        return false;
    char buf[512];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (n <= 0) return false;
    buf[n] = 0;
    out += buf;
    return true;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::puts("usage: chatcli <host> <port> <nick>");
        return 2;
    }
    const unsigned short port =
        static_cast<unsigned short>(std::atoi(argv[2]));

    opensamp::net::HandshakeClient c;
    if (!c.Start(argv[1], port, argv[3])) {
        std::puts("connect failed");
        return 1;
    }
    std::puts("connecting...");

    auto waitState = [&](opensamp::net::HandshakeClient::State s, int sec,
                         const char* what) {
        const auto dl = std::chrono::steady_clock::now() +
                        std::chrono::seconds(sec);
        while (std::chrono::steady_clock::now() < dl) {
            c.PollOnce();
            if (c.failed()) {
                std::printf("link failed while %s\n", what);
                return false;
            }
            if (c.state() == s) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        std::printf("timeout while %s\n", what);
        return false;
    };

    if (!waitState(opensamp::net::HandshakeClient::State::JoinSent, 15,
                   "joining"))
        return 1;
    c.RequestClass(0);
    const auto dlSpawn = std::chrono::steady_clock::now() +
                         std::chrono::seconds(15);
    while (std::chrono::steady_clock::now() < dlSpawn) {
        c.PollOnce();
        if (c.failed()) return 1;
        if (c.spawnReceived()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    if (!c.spawnReceived()) {
        std::puts("no spawn info");
        return 1;
    }
    c.RequestSpawn();
    std::printf("spawned as %s at (%.1f, %.1f, %.1f). "
                "Type chat, /pos, /q to quit.\n",
                argv[3], c.spawn().pos[0], c.spawn().pos[1],
                c.spawn().pos[2]);

    std::string pending;
    std::size_t shownChats = 0;
    auto next = std::chrono::steady_clock::now();
    bool quit = false;
    while (!quit) {
        c.PollOnce();
        if (c.failed()) {
            std::puts("disconnected");
            return 1;
        }
        // Incoming chat.
        const auto& lines = c.chatLines();
        while (shownChats < lines.size()) {
            const auto& ln = lines[shownChats++];
            std::printf("[%08x] %s\n", ln.color, ln.text_utf8.c_str());
            fflush(stdout);
        }
        // 20 Hz sync from latest known spawn.
        const auto now = std::chrono::steady_clock::now();
        if (now >= next) {
            const auto& sp = c.spawn();
            c.SendOnFoot(opensamp::net::makeIdleOnFoot(
                sp.pos[0], sp.pos[1], sp.pos[2], sp.rotation));
            c.SendAim(opensamp::net::makeIdleAim(sp.pos[0], sp.pos[1],
                                                sp.pos[2]));
            next = now + std::chrono::milliseconds(50);
        }
        // Keyboard.
        std::string chunk;
        if (stdinLine(chunk)) {
            pending += chunk;
            std::size_t nl;
            while ((nl = pending.find('\n')) != std::string::npos) {
                std::string line = pending.substr(0, nl);
                pending.erase(0, nl + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line == "/q") {
                    quit = true;
                } else if (line == "/pos") {
                    const auto& sp = c.spawn();
                    std::printf("me: (%.1f, %.1f, %.1f)\n", sp.pos[0],
                                sp.pos[1], sp.pos[2]);
                } else if (line == "/who") {
                    for (const auto& [id, rp] : c.remotes())
                        std::printf("  id=%u pos=(%.1f, %.1f, %.1f) hp=%u\n",
                                    id, rp.pos[0], rp.pos[1], rp.pos[2],
                                    rp.health);
                    if (c.remotes().empty()) std::puts("  (nobody seen yet)");
                } else if (!line.empty()) {
                    if (!c.SendChat(line.c_str()))
                        std::puts("chat send failed");
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    c.Shutdown();
    std::puts("bye");
    return 0;
}
