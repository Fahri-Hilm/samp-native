# samp-native — SAMP client, Linux native (opsi C)

Clean-room. Tidak tempel kode client SAMP asli. Tidak commit aset game Rockstar.
Lisensi target: GPL-3.0 (mengikuti OpenSamp: sampraknet hanya tawarkan opsi GPL).

Ref beku upstream: OpenSamp `a05717e` (2026-08-23), clone baca-saja di
`~/Projects/opensamp-riset`. Riset port: `~/Projects/opensamp-riset/RISET.md`.

## Isi

- `port/util/encoding.h` — CP1251↔UTF-8 tanpa `<Windows.h>`.
- `port/gui/color_parse.h` — tokenizer `{RRGGBB}` murni + length guard.
- `port/net/handshake.h` — `HandshakeClient`: join/auth/spawn/chat/sync/
  aim/death, roster, remote player+vehicle store. Multi-instance aman
  (thread_local routing).
- `port/net/chat.h` — codec RPC 101/93 + CP1251.
- `port/net/sync.h` — `OnFootData` 68B, `AimData` 31B, kirim 20 Hz.
- `port/net/sync_in.h` — decode sync pemain lain (quat/vector/nibble HP).
- `port/net/vehicle.h` — codec kendaraan kirim + terima.
- `port/cli/chatcli.cpp` — client terminal: join+spawn+sync+chat
  interaktif (`/pos /who /q`).

## Build & uji

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build        # unit saja (live di-skip otomatis)
```

Uji live lawan server lokal (open.mp Linux, allow_037_clients):

```
~/Projects/omp-server-ref/Server/omp-server &
SAMP_TEST_LIVE=1 SAMP_TEST_PORT=7777 ./build/test_handshake   # join-full
SAMP_TEST_LIVE=1 SAMP_TEST_PORT=7777 ./build/test_twoplayer  # A↔B saling lihat
./build/chatcli 127.0.0.1 7777 NickKamu                      # main teks
```

## Status protokol (terverifikasi live vs open.mp 1.6.7)

join, auth-key, ClientJoin, RequestClass→spawn-info, RequestSpawn+Spawn,
chat 2 arah, on-foot 20 Hz, aim 20 Hz, death/respawn, roster add/remove,
decode sync pemain lain + kendaraan. Lihat `MEMORY.md` + `ENGINE.md`.
Sisa: glue Unity (`GLUE.md`, butuh editor) + masuk-kendaraan live.
