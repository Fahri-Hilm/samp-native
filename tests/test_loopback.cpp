// Loopback: vendored RakServer -> vendored RakClient, 450-byte payload.
// Decides whether big-datagram loss is our-stack-on-Linux or open.mp-specific.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <vector>

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
            std::printf("[cli-conn] pid=%u len=%u\n", p->length ? p->data[0] : 0,
                        p->length);
            if (p->data[0] == ID_CONNECTION_REQUEST_ACCEPTED) connected = connected && true;
            cli->DeallocatePacket(p);
        }
        if (connected) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    check(connected, "connected");

    // Server blasts a user packet (size via BLOB_SIZE, default 450 —
    // mirrors open.mp init burst size).
    const char* sz = std::getenv("BLOB_SIZE");
    const int blobSize = sz ? std::atoi(sz) : 450;
    std::vector<char> blobv(static_cast<std::size_t>(blobSize));
    // First byte must be >= ID_RPC (20): RakNet delivers datagrams with a
    // smaller leading byte only via internal branches. Real SAMP traffic
    // always starts with a valid message id (RPC=20, sync=200+).
    blobv[0] = static_cast<char>(0x91);
    for (int i = 1; i < blobSize; ++i) blobv[i] = static_cast<char>(i * 7 + 1);
    const char* blob = blobv.data();
    const std::size_t blobLen = blobv.size();
    const char tiny[10] = "012345678";
    const char* swap = std::getenv("SWAP_ORDER");
    if (swap) {
        check(srv->Send(tiny, sizeof(tiny), HIGH_PRIORITY, RELIABLE, 0,
                        UNASSIGNED_PLAYER_ID, true), "server-send-tiny");
        check(srv->Send(blob, static_cast<int>(blobLen), HIGH_PRIORITY, RELIABLE, 0,
                        UNASSIGNED_PLAYER_ID, true), "server-send");
    } else {
        check(srv->Send(blob, static_cast<int>(blobLen), HIGH_PRIORITY, RELIABLE, 0,
                        UNASSIGNED_PLAYER_ID, true), "server-send");
        check(srv->Send(tiny, sizeof(tiny), HIGH_PRIORITY, RELIABLE, 0,
                        UNASSIGNED_PLAYER_ID, true), "server-send-tiny");
    }

    bool gotIt = false;
    bool gotTiny = false;
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        Packet* p = nullptr;
        while ((p = cli->Receive()) != nullptr) {
            std::printf("[cli] pid=%u len=%u\n", p->length ? p->data[0] : 0,
                        p->length);
            if (p->length == 100 || p->length == 450) {
                std::printf("[cli] first16:");
                for (unsigned i = 0; i < 16 && i < p->length; ++i)
                    std::printf(" %02x", p->data[i]);
                std::printf("\n");
            }
            if (p->length == blobLen &&
                std::memcmp(p->data, blob, blobLen) == 0)
                gotIt = true;
            if (p->length == sizeof(tiny) &&
                std::memcmp(p->data, tiny, sizeof(tiny)) == 0)
                gotTiny = true;
            cli->DeallocatePacket(p);
        }
        if (gotIt && gotTiny) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    check(gotIt, "big-payload-received");
    check(gotTiny, "tiny-payload-received");

    cli->Disconnect(100);
    RakNetworkFactory::DestroyRakClientInterface(cli);
    srv->Disconnect(100);
    RakNetworkFactory::DestroyRakServerInterface(srv);

    if (fails == 0) std::puts("ALL PASS");
    return fails == 0 ? 0 : 1;
}
