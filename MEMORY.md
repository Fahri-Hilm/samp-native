# MEMORY — samp-native (SAMP client Linux native, opsi C)

Terakhir diperbarui: 2026-11-06. Baca file ini sebelum melanjutkan pekerjaan.
Repo: https://github.com/Fahri-Hilm/samp-native (PUBLIC, branch `main`).
Lokal: `~/Projects/samp-native`. Bahasa: Indonesia.

## Goal

Main di server RP SAMP (dengan player lain) dari Linux native, nol Wine.

## Status: fondasi native HIJAU, belum bisa main

Yang terbukti jalan native (mesin CachyOS ini, terverifikasi):
- Engine SAU v4.0 Linux: boot, server+client headless connect, ped spawn,
  baca aset pack PS2. (Biner ref di `~/Projects/sauref`, bukan bagian repo.)
- Net RakNet fork SAMP: kompilasi + jalan native (`libraknet.a`).
- Handshake lawan open.mp 1.6.7 lokal: connect + auth + ClientJoin +
  RequestClass → server kirim 15 RPC. Nick `NativeTest` tercatat join di log server.
- `ctest 5/5 pass` (encoding, color, netlink, loopback, handshake-skip).

Yang BELUM: decode balasan RequestClass (spawn info) → RequestSpawn/Spawn →
spawn beneran → sync on-foot → glue ke SAU → main RP.

## Struktur repo

- `port/util/encoding.h` — CP1251↔UTF-8 tanpa `<Windows.h>`. Test vs codec CPython.
- `port/gui/color_parse.h` — tokenizer `{RRGGBB}` murni (tanpa ImGui) + length
  guard lawan OOB read upstream.
- `port/net/handshake.h` — `HandshakeClient`: AUTH_KEY, accept
  (playerID+challenge), `ClientJoin` (v4057, mod 1, nick, challenge^v, gpci,
  "0.3.7"), `RequestClass`, `RegisterAllRpcs()` 0–255 + counter.
- `tests/`: `test_encoding`, `test_color`, `test_netlink`, `test_loopback`
  (450B byte-exact old↔old), `test_handshake` (live, opt-in `SAMP_TEST_LIVE=1`).
- `ENGINE.md` — keputusan adopsi SAU + bukti + gap protokol.
- `CMakeLists.txt` — `add_subdirectory` ke vendor di clone riset
  (bukan copy). Flag `-include cstdint` (GCC 16).

## Repo terkait (tidak dipush)

- `~/Projects/opensamp-riset` — clone read-only OpenSamp ref `a05717e`
  + delta port Linux 40 baris (tercatat di `RISET.md`):
  `RakPeer.cpp` SEH guard + `delete[]` x2, `ReliabilityLayer.cpp` print
  diagnostik env-gated, `SingleProducerConsumer.h` ctor + `delete` typed.
  JANGAN push clone ini (bukan milik kita).
- `~/Projects/omp-server-ref` — open.mp server 1.6.7 Linux (loop uji handshake).
- `~/Projects/sauref` — biner SAU v4.0 Linux (ref engine).
- `~/Documents/PS 2 FARYY4U` — pack GTA user (aset `game_dir` SAU).
- Server RP user: `104.234.180.175:7777` (voice :8888) — via Wine `samp` utk skrg.

## Perintah

```
cd ~/Projects/samp-native
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
ctest --test-dir build                      # 5/5, handshake skip tanpa server
# live handshake:
cd ~/Projects/omp-server-ref/Server && ./omp-server &
SAMP_TEST_LIVE=1 SAMP_TEST_PORT=7777 ./build/test_handshake
```

## Temuan penting (jangan ulang diagnosis)

1. RPC masuk dimakan `HandleRPCPacket` bila tak ada handler terdaftar
   (drop `UNDEFINED_RPC_INDEX`) — client WAJIB register handler.
2. open.mp validasi gpci: `int(gpci,16) % 1001 == 0` — port `gen_gpci` valid.
3. open.mp kirim init burst sebagai RELIABLE_ORDERED ch2 (~15 RPC, id
   12/60/69/72/93/128/135/139/157/158).
4. `data[0] < ID_RPC(20)` di-drop drain `RakPeer` — payload test wajib
   diawali byte valid (pakai 0x91).
5. Bug port Linux yg sudah difix: scalar `delete` atas `new[]` (heap
   corruption flaky), node `SingleProducerConsumer` tak ter-init (paket
   hilang acak), dtor/Clear type mismatch.

## Langkah berikut (saat lanjut)

1. ~~Decode balasan RPC 128 (outcome + `PLAYER_SPAWN_INFO`) di `HandshakeClient`.~~
   SELESAI 2026-11-06: `OnRequestClass` + `SpawnInfo` pack(1)/46B, live vs
   open.mp → outcome=1, skin=0, pos=(-1291.7, 2513.8, 87.1), rot=355.4.
   Test assert outcome/finite/skin-range — hijau.
2. ~~Kirim `RequestSpawn`(129) + `Spawn`(52) → verifikasi spawn di log server.~~
   SELESAI 2026-11-06: `RequestSpawn()` kirim 129+52 kosong (mirip upstream).
   Live: server balas (RPC 129 tercatat), total RPC 15→23 (id baru 17/19/
   22/67/162), sesi hidup, part bersih tanpa kick/ban. Test assert
   spawn-survived + spawn-state — hijau.
3. ~~Bridge chat-only (kirim/terima chat teks) — tonggak "main teks".~~
   SELESAI 2026-11-06: `port/net/chat.h` (codec RPC 101/93 + CP1251) +
   `test_chat` hijau. Live: kirim "halo dari native" → tercatat di server
   log; terima 5 baris welcome gungame (warna terdecode). `ctest 6/6`.
4. ~~On-foot sync 20Hz → glue SAU (butuh Unity editor) → vehicle/textdraw.~~
   SELESAI sync 2026-11-06: `port/net/sync.h` (`OnFootData` 68B pack(1)) +
   `test_sync` hijau. Live: 196 sync/10s dari titik spawn, server balas
   RPC 67 x197, sesi hidup, part bersih. Glue SAU (Unity editor) sisa.
   SELESAI aim 2026-11-06: `AimData` 31B (zoom/weapon 1 byte) + live 196/10s.
   SELESAI lihat-pemain 2026-11-06: `port/net/sync_in.h` decode kompresi
   (quat/vector/nibble HP) + `test_twoplayer`: A(id0)↔B(id1) saling lihat
   di posisi spawn. `ctest` 9 test (rilis: `ctest 10/10` + live hijau).
   SELESAI relay Unity 2026-11-07: player Unity SAU (native, build lokal)
   join + announced chat + push sync 20 Hz; observer independen melihat
   `id=0 pos=(-1290,2512) hp=100`. "Tidak relay" sebelumnya = jarak
   stream (±3400 unit), BUKAN bug. Hook uji: `SAMP_FORCE_POS`.
   SELESAI master plan 2026-11-07: `docs/master-plan.md` adopsi v2.0 +
   amandemen status teruji (engine=SAU, protocol done, ADR-0017/18/19).
   SELESAI crash rilis 2026-11-06: loopback SEGFAULT khusus -O2 —
   akar: GCC sized array-delete baca cookie di p-8 tanpa cek null
   (disassembly), dipicu hapus array null. Fix: guard `if(listArray)`
   + flag build `-fno-delete-null-pointer-checks` (bukti bisect: cuma
   flag ini yang menyembuhkan). Bonus MSan: struct `RemoteSystem`
   tak-terinit + `char c[3]` uninit di handshake offline → di-fix.
   SELESAI roster 2026-11-06: decode WorldPlayerAdd/Remove (32/163),
   two-player assert roster silang, `/who` chatcli tampilkan roster.
   SELESAI glue C# 2026-11-06: `port/net/samp_native_api.h/.cpp` +
   `libsamp_native.so` (18 simbol C, `nm -D` OK): `test_api` unit +
   `test_api_live` hijau (join/chat/sync/push via .so — artefak yang
   akan di-P/Invoke Unity). C# `glue/SampSharp` (`SampBridge` MonoBehaviour
   Mirror-free + P/Invoke + stub API SAU/Unity): `dotnet build` hijau.
   Belum diuji in-engine (butuh Unity editor) — lihat `glue/README.md`
   untuk drop-in steps.
   SELESAI vehicle 2026-11-06: `port/net/vehicle.h` encode (client→server)
   + decode (server→client) + `test_vehicle` hijau. Temuan: format kirim
   vs terima beda (server prepend playerId + kompres) — sama polanya
   dengan on-foot. Live test masuk-kendaraan pending (butuh dunia).
   + passenger 24B / unoccupied 67B / markers decode + unit hijau.
   SELESAI chatcli 2026-11-06: `port/cli/chatcli.cpp` — client terminal
   native (join+spawn+sync 20Hz+chat interaktif, /pos /who /q). Live:
   join, welcome tampil, chat terkirim ke server, quit bersih.
