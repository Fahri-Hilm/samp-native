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
        // Incoming dialog.
        static unsigned short shownDlg = 0xFFFF;
        if (c.hasDialog() && c.dialog().id != shownDlg) {
            shownDlg = c.dialog().id;
            const auto& d = c.dialog();
            std::printf("--- dialog %u style %u ---\n%s\n[%s] [%s]\n%s\n",
                        d.id, d.style, d.title_utf8.c_str(),
                        d.button1_utf8.c_str(), d.button2_utf8.c_str(),
                        d.info_utf8.c_str());
            std::puts("(reply: /dresp <0|1> [text], /dlist <n> for lists)");
            fflush(stdout);
        }
        if (!c.hasDialog()) shownDlg = 0xFFFF;
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
                    for (const auto& [id, rp] : c.remotes()) {
                        const auto it = c.roster().find(id);
                        const char* nm =
                            (it != c.roster().end() && it->second.name[0])
                            ? it->second.name
                            : "?";
                        std::printf("  id=%u %s pos=(%.1f, %.1f, %.1f) hp=%u\n",
                                    id, nm, rp.pos[0], rp.pos[1], rp.pos[2],
                                    rp.health);
                    }
                    for (const auto& [id, re] : c.roster())
                        if (c.remotes().count(id) == 0)
                            std::printf("  id=%u skin=%d (not in range)\n",
                                        id, re.skin);
                    if (c.remotes().empty() && c.roster().empty())
                        std::puts("  (nobody seen yet)");
                } else if (line.rfind("/dresp ", 0) == 0) {
                    // /dresp <btn 0|1> [text...]
                    int btn = 1;
                    std::string txt;
                    {
                        std::string rest = line.substr(7);
                        auto sp2 = rest.find(' ');
                        std::string b = (sp2 == std::string::npos)
                                           ? rest
                                           : rest.substr(0, sp2);
                        if (!b.empty() && (b[0] == '0' || b[0] == '1'))
                            btn = b[0] - '0';
                        if (sp2 != std::string::npos) txt = rest.substr(sp2 + 1);
                    }
                    if (!c.hasDialog()) {
                        std::puts("(no active dialog)");
                    } else if (!c.SendDialogResponse(
                                   c.dialog().id,
                                   static_cast<std::uint8_t>(btn), 0xFFFF,
                                   txt.c_str())) {
                        std::puts("dialog send failed");
                    } else {
                        std::printf("(dialog %u answered btn=%d)\n",
                                    c.dialog().id, btn);
                    }
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
