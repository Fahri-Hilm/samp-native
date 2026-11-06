# samp-native — SAMP client, Linux native (opsi C)

Clean-room. Tidak tempel kode client SAMP asli. Tidak commit aset game Rockstar.
Lisensi target: GPL-3.0 (mengikuti OpenSamp: sampraknet hanya tawarkan opsi GPL).

Ref beku upstream: OpenSamp `a05717e` (2026-08-23), clone baca-saja di
`~/Projects/opensamp-riset`. Riset port: `~/Projects/opensamp-riset/RISET.md`.

## Isi

- `port/util/encoding.h` — port `native/util/encoding.h`: CP1251↔UTF-8 tanpa
  `<Windows.h>` (tabel statis + codec UTF-8 sendiri). API sama persis.
- `tests/` — vektor dari codec CPython (acuan) + `test_encoding`.

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

## Berikutnya

Parser warna chat (`colored_text.h::ParseHexColorTag`, pure) → `port/gui/`.
Lalu interface `IGameState`/`INetSender`/`IChatSink`/`IDialogSink` untuk
memutus siklus bridge↔gui dan gantungan `game.h`.
