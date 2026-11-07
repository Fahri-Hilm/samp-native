# MASTER PLAN — Adopsi + Status (samp-native)

Dokumen ini mengadopsi "MASTER PLAN LENGKAP v2.0" (teks lengkap terlampir
di Bagian B) menjadi rencana kerja repo ini, dengan amandemen berdasarkan
hasil implementasi yang sudah terverifikasi di mesin ini
(CachyOS/Arch Linux x86-64, NVIDIA RTX 3050 + Intel iGPU).

Aturan baca: Bagian A = keputusan final + status teruji (mengikat).
Bagian B = teks v2.0 asli sebagai referensi visi jangka panjang.
Jika bertentangan, Bagian A menang.

================================================================================
BAGIAN A — AMANDEMEN ADOPSI (status terverifikasi)
================================================================================

A0. STATUS KESELURUHAN (per 7 Nov 2026)
- Protocol multiplayer-nya SUDAH jalan native dan terverifikasi live:
  join, auth, spawn, chat 2 arah, sync on-foot 20 Hz, aim, death/respawn,
  roster, lihat-pemain-lain dua arah, relay chat antar-pemain.
- Bukti: `ctest` hijau + live test vs open.mp 1.6.7 lokal
  (`test_handshake`, `test_twoplayer`, `test_api_live`).
- Engine 3D: SanAndreasUnity v4.0 (MIT) TERBUKTI boot native di mesin ini,
  dan build SAU + glue SAMP kita TERBUKTI join server + kirim chat + kirim
  sync dari dalam dunia 3D (lihat A2).
- Yang BELUM: render terlihat mata (headless; butuh uji grafis),
  visual ped lokal/remote di SAU, masuk kendaraan di SAU,
  names on-wire (open.mp tidak mengirim nama di flow ini).

A1. KEPUTUSAN ENGINE (menggantikan §04-librw, §05-render custom, §08-OpenGL)
- Jalur utama = SanAndreasUnity + glue SAMP native (`glue/`,
  `port/net/samp_native_api.{h,cpp}` -> `libsamp_native.so`).
  BUKAN menulis ulang engine RenderWare dari nol.
- Alasan: SAU MIT, Linux build terbukti jalan di mesin ini, render +
  fisika + streaming GTA SA sudah ada; yang kurang hanya backend
  multiplayer SAMP — itulah yang kita bangun (`port/net/*`).
- librw/gta-reversed turun status jadi REFERENSI format/asset bila SAU
  kurang (sesuai semangat §04 tapi tanpa komitmen build engine sendiri).
- Unity 2020.3.26f1 (exact SAU) berjalan di CachyOS ini dengan catatan:
  butuh `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1` (ICU CoreCLR lama),
  `openssl-1.1` (libssl.so.1.1 untuk Roslyn), `libxml2-legacy`
  (libxml2.so.2), dan PulseAudio session untuk FMOD headless.
- Lisensi editor: Personal gratis via aktivasi online di aplikasi Hub
  (offline .ulf TIDAK berlaku untuk Personal — fakta dari portal Unity).

A2. BUKTI INTEGRASI SAU (terverifikasi, bukan rencana)
- Player SAU hasil build lokal + `SampBridge` (auto-start, tanpa edit scene)
  + `libsamp_native.so`: join server open.mp lokal sebagai `SAUPlayer`,
  kirim chat tampil di server log, kirim sync 20 Hz terukur via tcpdump,
  diterima + di-relay server (terbukti: observer native melihat
  `id=0 pos=(-1290,2512) hp=100` persis di posisi paksa Unity player).
- Kesalahpahaman awal ("server tidak relay") = jarak stream, bukan bug:
  spawn acak gungame menaruh pemain ±3400 unit dari observer.
- Hook uji: `SAMP_FORCE_POS="x y z [rot]"` (env, hanya untuk testing).

A3. PROTOKOL (menggantikan §10/§19-fase-network sebagai "rencana")
- SELESAI + live-teruji: handshake/auth-key/challenge, ClientJoin(25),
  RequestClass(128)+decode spawn, RequestSpawn(129)+Spawn(52),
  chat 101/93 + relay decode, on-foot 207 kirim+terima (kompresi),
  aim 203, death 53, roster WorldPlayerAdd/Remove (32/163),
  vehicle/passenger/unoccupied/markers decode, name via ServerJoin(137
  handler siap; open.mp tidak mengirimnya di flow ini — chat bawa nama).
- Perbedaan format kirim vs terima TERDOKUMENTASI di kode
  (server prepend playerId + kompres; client kirim mentah).
- Server uji: `~/Projects/omp-server-ref` (open.mp 1.6.7 Linux native).
- Catatan bug toolchain yang DIPERBAIKI (bukan teori):
  sized array-delete null di -O2 (-> `-fno-delete-null-pointer-checks`),
  `delete` vs `delete[]` campur, `SingleProducerConsumer` node tak
  ter-init, `RemoteSystemStruct` tak ter-init, uninit handshake bytes.

A4. MODDING (menajamkan §11/§12/§13 sesuai realita)
- ModLoader folder-semantics via VFS = target, BELUM dimulai (jalan benar:
  setelah visual ped + kendaraan di SAU stabil).
- MoonLoader .lua langsung = TIDAK didukung di ELF (butuh port/API —
  sesuai klasifikasi D di dokumen v2.0, bukan janji).
- ReShade FX = bertahap per-efek setelah render target stabil.
- ASI/DLL Windows = tidak dieksekusi (batas arsitektur, sesuai ADR-0011).

A5. TOOLCHAIN SAAT INI (menggantikan asumsi §18 generik)
- `port/*` C++17, CMake, `ctest` (unit) + live test berpintu env
  (`SAMP_TEST_LIVE=1` + server lokal; handshake/twoplayer/api_live).
- ASan/UBSan/MSan bersih untuk jalur yang diuji; flag release wajib:
  `-fno-delete-null-pointer-checks` (terbukti via bisect).
- Unity 2020.3.26f1 headless: import + `SampBuild.BuildLinux` hijau.
- Peta repo: `port/{util,gui,net,cli}` + `glue/{SampSharp,Stubs}` +
  `tests/` + `docs/` (file ini) + `ENGINE.md` + `GLUE.md` + `MEMORY.md`.

A6. DEFINISI "MAIN SAMA PLAYER LAIN" (target terima-beres, terukur)
- DONE: dua/tiga client native saling lihat posisi + HP + chat + roster
  di server uji (nama pemain: TIDAK tersedia on-wire di flow open.mp;
  roster tampilkan id; chat tampilkan nama pengirim dari teks).
- DONE: client Unity 3D join + chat + sync (observer independen
  memverifikasi posisi Unity player).
- SISA menuju "main normal": visual ped (spawn prefab di SAU +
  gerakkan kamera ikut), masuk kendaraan, uji grafis di layar
  (bukan headless), lalu uji ke server RP publik bila diizinkan.

================================================================================
BAGIAN B — TEKS MASTER PLAN LENGKAP v2.0 (dokumen visi, tidak diubah)
================================================================================

Tanggal dokumen : 7 November 2026
Platform utama  : CachyOS / Arch Linux, x86-64, Wayland dan X11
Target output   : Executable ELF Linux asli; tidak menggunakan Wine/Proton
Bahasa utama    : C++20
Strategi        : Compatibility-first, vertical-slice-first, measurable optimization
Nama resmi      : Nama resmi belum ditetapkan; nama teknis sementara "samp-native"

PENTING (tetap berlaku):
- Ini rancangan pengembangan; klaim kompatibilitas mengikuti definisi
  kompatibilitas di §21, bukan janji universal.
- "Semua server" dan "semua MoonLoader/modpack" adalah VISI JANGKA PANJANG.
- Aset GTA SA dari instalasi berlisensi milik pengguna; jangan didistribusikan.
- Tidak optimasi/bypass anti-cheat; patuhi aturan server.

--- ISI UTAMA v2.0 (ringkas terstruktur; detail penuh milik penulis visi) ---

01. Ringkasan strategi: hybrid open-source + native C++, SDL3, librw PoC,
    RenderWare/playability vertical slice, multi-track paralel
    (world, network, mods, QA). -> DIADOPSI dengan amandemen A1
    (engine = SAU, bukan SDL3+librw custom).

02. Visi/target/non-goals: P0 native ELF + aset legal + SA-MP/open.mp +
    RP fundamental + terukur; P1 ModLoader/Lua/0.3.DL/FX; P2 Vulkan/dll.
    -> DIADOPSI. Tambahan status: P0-network SELESAI teruji; P0-3D
    sebagian (engine jalan, visual/interaksi dalam kerja).

03. Prinsip: vertical slice, protocol-first, compatibility-first, ukur
    dulu, VFS dini, crash isolation, replayability. -> DIADOPSI penuh.

04. Fondasi open-source (librw, gta-reversed, OpenSAMP, open.mp, RakNet
    fork, ModLoader, MoonLoader docs, ReShade, vkBasalt, SAU + checklist
    adopsi + lisensi). -> DIADOPSI dengan perubahan: SAU naik dari
    "inspirasi" menjadi ENGINE UTAMA (terbukti); gta-reversed tetap
    referensi; OpenSAMP = referensi wire protocol (tidak ditempel;
    implementasi kita clean-room dari perilaku terobservasi).

05. Arsitektur end-to-end (launcher, platform host, runtime, renderer,
    gameplay, network, mod host, VFS, event bus, thread model).
    -> DIADOPSI sebagai arah; implementasi saat ini: network + sebagian
    gameplay/net glue (SAU menutup renderer/world/gameplay).

06. Modul + kontrak API (platform/foundation/vfs/assets/render/world/
    gameplay/net/ui/mods/launcher/tools + stable C ABI).
    -> DIADOPSI; yang sudah ada: net/* + sebagian foundation; C ABI
    stabil parsial di `port/net/samp_native_api.h` (18 simbol).

07. Asset loader/streaming (IMG/DFF/TXD/COL/IDE/IPL/IFP/DAT/CFG; no full
    SCM; pipeline index->VFS->parse->cache->stream->GPU upload->LOD).
    -> DITUNDA mengikuti SAU (loader SAU yang dipakai). VFS native
    tetap roadmap untuk ModLoader (A4).

08. Render pipeline (OpenGL dulu, abstraksi backend, MVP features,
    Vulkan gate ketat). -> DIAMENDEMEN: renderer = Unity/SAU;
    Vulkan gate tetap berlaku bila SAU pipeline dievaluasi ulang.

09. Gameplay runtime (state, ped, kamera, animasi, kendaraan, collision,
    input, audio). -> DIDELEGASIKAN ke SAU; fokus glue: spawn visual,
    transform sync, kamera follow, enter vehicle.

10. Networking SA-MP/open.mp (N0 open.mp lokal -> N1 0.3.7 -> N2 0.3.DL;
    handshake, RPC typed, sync, textdraw/dialog, validasi, fairness).
    -> SEBAGAIAN BESAR SELESAI (N0+N1 core: lihat A3). Sisa: 0.3.DL,
    textdraw/dialog/object/pickup decode, vehicle-enter flow.

11. ModLoader/VFS native (overlay order, phased M1-M6, mod manager,
    batas .asi/.dll). -> ROADMAP, belum dimulai (setelah visual+drive).

12. MoonLoader/LuaJIT compatibility (tier L0-L6, sandbox, conformance).
    -> ROADMAP JANGKA PANJANG; status E (dievaluasi per API).

13. ReShade FX/graphics mods (kategori G1-G5, compiler native, vkBasalt
    notes, preset, safe mode). -> ROADMAP setelah render target stabil.

14. Launcher/config/profil (XDG paths, server browser, presets, safe
    mode, packaging Arch). -> SEBAGIAN: `chatcli` = CLI proof;
    launcher GUI = nanti.

15. Keamanan/privasi (threat model 7 item + controls). -> BERLAKU;
    diterapkan bertahap (parser bounds sudah; fuzzing/sandbox = nanti).

16. Legalitas (aset proprietary, lisensi per dep, SBOM, no bypass).
    -> BERLAKU PENUH. Catatan: مشروع ini clean-room dari perilaku
    wire; RakNet fork = vendored (butuh audit lisensi final sebelum
    rilis publik); SAU MIT; Unity editor free Personal (runtime
    player bebas royalti untuk proyek ini).

17. Struktur repo/folder + runtime data XDG. -> DISESUAIKAN ke struktur
    aktual repo (lihat README.md); konvergensi penuh saat VFS/launcher
    dibangun.

18. Toolchain/QA/CI (C++20, CMake+Ninja, sanitizers, fuzz, metrics).
    -> BERLAKU; implementasi berjalan (sanitizers dipakai buktikan bug;
    CI server = nanti).

19. Roadmap Phase 0-10 + parallelism rules. -> STATUS: Phase 1 track
    network SELESAI (melebihi PoC: full join→play loop + multi-client);
    Phase 1 track render SELESAI via SAU (player build + join server);
    Phase 2-3 (VFS/world/playable character) didelegasikan/diintegrasi
    via SAU; Phase 4 vertical slice multiplayer = TERBUKTI di level
    protokol + visual Unity (glue sync visual = berjalan).

20. MVP slices MVP-0..MVP-8 (MVP-5 dua client saling lihat = milestone
    paling penting). -> MVP-4 + MVP-5 SELESAI TERUJI (headless + Unity).
    MVP-6 sebagian (chat ya; dialog/checkpoint/vehicle = antre).

21. Matriks kompatibilitas A/B/C/D/E/X + format katalog + kebijakan
    "all mods". -> BERLAKU; katalog diisi bertahap (mulai: open.mp
    1.6.7/Linux = A untuk fitur teruji; .asi/.dll Windows = D).

22. Performance engineering + benchmark fairness + anti-optimasi.
    -> BERLAKU PENUH. Belum ada klaim FPS (sesuai aturan).

23. QA/tests/conformance/regression + release blockers. -> BERLAKU;
    berjalan (unit + live gated + golden vectors via replays manual).

24. Risiko R1-R12 + stop/pivot conditions. -> BERLAKU; update:
    R1 (scope renderer) DITUTUP via adopsi SAU; R2 (protokol) DITUTUP
    untuk 0.3.7-core; R7 (lisensi fork) tetap TERBUKA (audit sebelum
    rilis publik).

25. Backlog P0/P1/P2 + sengaja ditunda. -> DIGANTI backlog aktual
    di MEMORY.md ("Langkah berikut") — lebih kecil & teruji per item.

26. Definition of Done + release labels. -> BERLAKU PENUH.

27. Ritme review/workflow/board/artefak. -> BERLAKU; board = MEMORY.md
    + git log per milestone.

28. Fitur lanjutan/ditunda. -> BERLAKU.

29. ADR-0001..ADR-0016 + success definition. -> DIADOPSI + tambahan:
    ADR-0017: engine = SAU (bukan custom RenderWare) — ACCEPTED
    (terbukti boot+join+sync 7 Nov 2026).
    ADR-0018: Unity 2020.3.26f1 toolchain Linux terkunci untuk SAU —
    ACCEPTED (build hijau).
    ADR-0019: `-fno-delete-null-pointer-checks` wajib di build Linux
    untuk vendored RakNet — ACCEPTED (bukti bisect crash rilis).

30. Sumber/rujukan [1]-[12]. -> BERLAKU; tambah:
    [13] Unity 2020.3.26f1 Linux editor (toolchain, Personal free).
    [14] Bukti live: svlog/chat/server-log + tcpdump + ctest di repo
    dan log sesi (bukan klaim tanpa artefak).

PESAN PENUTUP (tetap): BUAT YANG BISA DIMAINKAN, BUKAN HANYA
ARSITEKTUR YANG MENARIK. Target pertama = native ELF + Los Santos +
player controller + headless network validated + 2 clients sync.
Status kita: network-nya done; visual dunianya via SAU; ped/enter-vehicle
= langkah berikut yang konkret dan kecil.
