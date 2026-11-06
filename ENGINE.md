# Engine — keputusan: adopsi SanAndreasUnity (terbukti jalan native)

Tanggal: 2026-11-06. Referensi biner: `~/Projects/sauref` (SAU v4.0 Linux, MIT).
Bukan kode kita — hanya bukti jalan. Jangan commit biner ke repo ini.

## Bukti (di mesin ini, nol Wine)

- `SanAndreasUnity.x86_64 -batchmode -nographics` boot native (Mono, Mirror net, worker threads).
- Server headless: `Server: listening port=7778`, `config.user.json` → `game_dir` = pack PS 2 FARYY4U milik pengguna, `dontLoadTextures: true`.
- Client headless connect 127.0.0.1: server log `Player (netId=2) connected`,
  `Spawned ped (netId=3) for player`. Client log `Started loading GTA`,
  `Loading image archive: .../models/player.img`, 44+ model dimuat.
- Log: `/tmp/sau-server2.log`, `/tmp/sau-client.log`.

## Gap ke goal (main di server RP SAMP)

SAU bicara protokol Mirror miliknya — **tidak bisa join server SA-MP/open.mp**.
Yang harus dibangun: modul client protokol SAMP di atas SAU:

```
SAU (render dunia + fisika + ped, MIT)
  ↕ glue (spawn/pos/chat/dialog dipetakan ke API SAU)
port/ (milik kita, target GPL-3.0): encoding ✓, color_parse ✓,
  → IGameState/INetSender/IChatSink/IDialogSink (belum)
  → sampraknet portable + handshake/auth SAMP (belum)
```

Gabungan MIT+GPL = distribusi ikut GPL. Pakai pribadi: aman.

## Urutan kerja

1. Pengguna jalan SAU grafis sekali, pilih `game_dir` pack PS2 → konfirmasi dunia render (GPU real, bukan nographics).
2. Petakan lapisan net SAU (titik glue) dari sumbernya.
3. Handshake/auth SAMP portabel (uji lawan server lokal open.mp).
   - SELESAI 2026-11-06 fondasi: `libraknet.a` native terbuild (commit `7b39231`,
     test `netlink` BitStream + RakClient create/destroy hijau, **ctest 3/3**).
     Delta port: SEH guard `RakPeer.cpp` + `-include cstdint` (tercatat di clone).
4. Bridge chat-only dulu (teks jalan sebelum 3D sync).
   - SELESAI fondasi join 2026-11-06: `HandshakeClient` connect + auth +
     `ClientJoin` + `RequestClass` lawan open.mp 1.6.7 lokal → server
     kirim 15 RPC (id 12, 60, 69, 72, 93, 128, 135, 139, 157, 158).
     Temuan penting: RPC masuk dimakan `HandleRPCPacket` bila tak ada
     handler terdaftar — client wajib `RegisterAllRpcs()`.
   - Port fix RakNet di Linux (clone riset, delta 40 baris): `delete[]`
     (DeallocatePacket + buffered send), ctor `DataPlusPtr`, dtor/Clear
     `SingleProducerConsumer`, + print diagnostik env-gated.
5. On-foot sync 20Hz → vehicle → textdraw (urutan sama seperti upstream OpenSamp).
