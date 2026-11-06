// Loopback: vendored RakServer -> vendored RakClient, 450-byte payload.
// Decides whether big-datagram loss is our-stack-on-Linux or open.mp-specific.
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>

#include "RakNetworkFactory.h"
#include "RakServerInterface.h"
#include "RakClientInterface.h"
#include "BitStream.h"
#include "PacketEnumerations.h"

void Log(const char*, ...) {}

namespace {
int fails = 0;
void check(bool ok, const char* what) {
    if (!ok) { ++fails; std::printf("FAIL %s\n", what); }
}
} // namespace

int main() {
    RakServerInterface* srv = RakNetworkFactory::GetRakServerInterface();
    check(srv != nullptr, "server-create");
    check(srv->Start(4, 0, 0, 19132), "server-start");

    RakClientInterface* cli = RakNetworkFactory::GetRakClientInterface();
    check(cli != nullptr, "client-create");
    check(cli->Connect("127.0.0.1", 19132, 0, 0, 5), "client-connect");

    // Wait for the connection to establish on both ends.
    bool connected = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        Packet* p = nullptr;
        while ((p = srv->Receive()) != nullptr) {
            if (p->data[0] == ID_NEW_INCOMING_CONNECTION) connected = true;
            srv->DeallocatePacket(p);
        }
        while ((p = cli->Receive()) != nullptr) {
            if (p->data[0] == ID_CONNECTION_REQUEST_ACCEPTED) connected = connected && true;
            cli->DeallocatePacket(p);
        }
        if (connected) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    check(connected, "connected");

    // Server blasts a 450-byte user packet (mirrors open.mp init burst size).
    char blob[450];
    for (int i = 0; i < 450; ++i) blob[i] = static_cast<char>(i * 7 + 1);
    check(srv->Send(blob, sizeof(blob), HIGH_PRIORITY, RELIABLE, 0,
                    UNASSIGNED_PLAYER_ID, true), "server-send");

    bool gotIt = false;
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        Packet* p = nullptr;
        while ((p = cli->Receive()) != nullptr) {
            if (p->length == 450 + 1 &&
                std::memcmp(p->data + 1, blob, sizeof(blob)) == 0)
                gotIt = true;
            cli->DeallocatePacket(p);
        }
        if (gotIt) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    check(gotIt, "big-payload-received");

    cli->Disconnect(100);
    RakNetworkFactory::DestroyRakClientInterface(cli);
    srv->Disconnect(100);
    RakNetworkFactory::DestroyRakServerInterface(srv);

    if (fails == 0) std::puts("ALL PASS");
    return fails == 0 ? 0 : 1;
}
