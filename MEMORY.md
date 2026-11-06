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

1. Decode balasan RPC 128 (outcome + `PLAYER_SPAWN_INFO`) di `HandshakeClient`.
2. Kirim `RequestSpawn`(129) + `Spawn`(52) → verifikasi spawn di log server.
3. Bridge chat-only (kirim/terima chat teks) — tonggak "main teks".
4. On-foot sync 20Hz → glue SAU (butuh Unity editor) → vehicle/textdraw.
