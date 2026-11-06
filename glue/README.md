# SampSharp — SAMP backend C# untuk San Andreas Unity

Status: **kode terverifikasi kompilasi** (`dotnet build` hijau lawan stub
API yang ditranskrip dari source SAU), **belum diuji in-engine**
(butuh Unity editor — lihat bawah).

## Arsitektur

Mirror SAU tidak dipakai sama sekali untuk SAMP. `SampBridge`
(MonoBehaviour, tanpa NetworkBehaviour) bicara ke `libsamp_native.so`
lewat P/Invoke (`SampNative.cs`) dan menggerakkan visual SAU yang sudah
ada:

| SAMP event | Aksi SAU |
|---|---|
| server/player chat | `ChatManager.singleton.OnChatMessageReceivedOnLocalPlayer(new ChatMessage(text, sender))` → tampil di `ChatDisplay` bawaan |
| spawn confirmed | `Instantiate(PedManager.Instance.pedPrefab)` + `PlayerModel.Load(skin)` |
| tiap frame | transform ped lokal → `samp_set_local_pos` + `samp_push_state` (20 Hz di dalam .so) |
| sync pemain lain | satu visual Ped per id SAMP, `transform.position` lerp |
| pemain hilang | `Object.Destroy` + hapus dari map |
| chat keluar | `SampBridge.SendChat(msg)` — ganti pemanggilan `ChatSync.SendChatMsgToServer` di `ChatInputController` |

String UTF-8 disusun manual (`SampMarshal`: byte[] + NUL untuk kirim,
IntPtr→UTF8 untuk terima) karena netstandard2.0 tidak punya LPUTF8Str —
layout biner identik dengan yang dipakai Unity Mono.

## Cek kompilasi tanpa Unity

```
cd glue && ~/dotnet/dotnet build SampSharp.csproj
```

`Stubs/` = permukaan API Unity + SAU yang dipakai glue, ditranskrip dari
source SAU (`Ped.PlayerModel`, `PedModel.Load(int)`,
`PedManager.Instance.pedPrefab`, `ChatManager.singleton`,
`ChatMessage(msg, sender)`). Lolos build = pemakaian API konsisten
dengan source SAU, BUKAN bukti jalan in-engine.

## Drop-in ke Unity (saat editor tersedia)

1. Buka project SanAndreasUnity dengan Unity 2020.3.26+.
2. Copy `glue/SampSharp/*.cs` ke `Assets/Scripts/SAMP/` (tanpa `Stubs/`!).
3. Copy `build/libsamp_native.so` ke `Assets/Plugins/x86_64/`.
4. Taruh `SampBridge` di scene Startup, isi host/port/nick.
5. Nonaktifkan SAU Mirror networking untuk scene ini (glue jalan
   Mirror-free; `NetManager`/`NetworkServer` tidak dipakai).
6. Ganti `ChatInputController` → panggil `SampBridge.SendChat`.
7. Play: join → spawn → chat → lihat player lain.

## Batasan jujur

- Skin ped lokal pakai `fallbackSkinId` (skin sendiri tidak dikirim
  server dalam flow ini); remote pakai skin dari WorldPlayerAdd.
- Tanpa test in-engine: pergerakan kamera mengikuti ped lokal perlu
  verifikasi (SAU camera follow target default).
- Animasi remote ped belum dipetakan (pos + rotasi saja).
