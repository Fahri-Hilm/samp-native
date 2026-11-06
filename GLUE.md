# SAU glue — titik sambung SAMP di atas SanAndreasUnity

Ref SAU: `~/Projects/SanAndreasUnity` (clone depth-1, MIT, branch dev).
Dibaca, tidak diubah. Glue butuh Unity 2020.3.26+ (belum ada di mesin ini) —
dokumen ini peta kerja, bukan kode terverifikasi.

## Model net SAU (Mirror) vs SAMP

SAU: client kirim INPUT (`PedSync.SendInput`), server simulasi, transform
disebar (`TransformSyncer`). SAMP: client kirim POSISI (`ID_PLAYER_SYNC`
20Hz), server sebar apa adanya. Glue = backend SAMP yang memotong jalur
Mirror, memakai ulang render/fisika/kamera SAU.

## Seam (file SAU → fungsi kita)

1. Chat masuk: `Chat/ChatSync.cs` → `ChatManager.singleton
   .OnChatMessageReceivedOnLocalPlayer(new ChatMessage(msg, sender))`.
   Panggil ini dari decode `ClientMessage(93)` milik kita
   (`port/net/chat.h` sudah cocok: teks UTF-8 + warna).
2. Chat keluar: cegat `ChatSync.SendChatMsgToServer` → `SendChat()`
   (`port/net/handshake.h`, RPC 101 + CP1251).
3. Spawn lokal: `Ped/Ped_Spawning.cs` → `Ped.SpawnPed(pedId, pos, rot,
   spawnOnNetwork)`. Skin SAMP (0-311) → `pedId` SAU (`PedestrianDef`).
   Kamera SAU sudah mengikuti `Player.OwnedPed`.
4. Ped remote: satu `Ped` per SAMP playerId (spawn client-side dari
   `PedManager.Instance.pedPrefab`), tiap sync update transform:
   pos + yaw→Quaternion + anim id. Hapus saat `WorldPlayerRemove(163)`.
5. Kendaraan: `VehicleSync.cs` sudah ada polanya; SAMP `INCAR_SYNC`
   (200) dipetakan ke sana (belum dikerjakan di sisi kita).
6. Konek/putus: ganti `CustomNetworkManager`/`NetManager` dengan backend
   SAMP (`port/net/handshake.h` + RakNet native). Server SAU tidak dipakai
   untuk RP — servernya tetap SAMP/open.mp.

## Urutan implementasi (saat Unity tersedia)

1. Backend SAMP headless di Unity (C#): konek + join + spawn + chat
   memakai API `port/net` via P/Invoke ke `.so` native (atau port C#).
2. Tampilkan chat SAMP di `ChatDisplay` (seam 1-2). Tonggak visual pertama.
3. Spawn ped lokal + kamera (seam 3). Jalan-jalan di dunia RP.
4. Remote ped + lerp transform (seam 4). Lihat player lain.
5. Kendaraan (seam 5) → main penuh.

## Status

Backend protokol (`port/net`) hijau dan live-teruji lawan open.mp.
Yang kurang hanya lapisan Unity di atasnya. Tanpa Unity editor di mesin
ini, glue tidak bisa dikompilasi/ditesting — jangan klaim jalan.
