// Smoke test: vendored sampraknet links and runs natively on Linux.
// Covers BitStream write/read round-trip + RakClient create/destroy.
#include <cstdio>

#include "RakNetworkFactory.h"
#include "RakClientInterface.h"
#include "BitStream.h"

// sampraknet's RakPeer.cpp expects this symbol (upstream: bridge log).
void Log(const char* /*fmt*/, ...) {}

namespace {
int fails = 0;
void check(bool ok, const char* what) {
    if (!ok) { ++fails; std::printf("FAIL %s\n", what); }
}
} // namespace

int main() {
    RakClientInterface* c = RakNetworkFactory::GetRakClientInterface();
    check(c != nullptr, "client-create");
    if (c) {
        check(!c->IsConnected(), "not-connected");
        RakNetworkFactory::DestroyRakClientInterface(c);
    }

    RakNet::BitStream bs;
    const unsigned char w8 = 0xAB;
    const unsigned w32 = 0xDEADBEEFu;
    bs.Write(w8);
    bs.Write(w32);
    bs.Write(true);
    check(bs.GetNumberOfBytesUsed() == 1 + 4 + 1, "size");

    bs.ResetReadPointer();
    unsigned char r8 = 0;
    unsigned r32 = 0;
    bool rb = false;
    check(bs.Read(r8) && r8 == w8, "u8");
    check(bs.Read(r32) && r32 == w32, "u32");
    check(bs.Read(rb) && rb, "bool");

    if (fails == 0) std::puts("ALL PASS");
    return fails == 0 ? 0 : 1;
}
