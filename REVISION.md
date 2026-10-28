# REVISION — wt32-eth01-pc-control

Tinjauan kode (review statis + compile `esp32:esp32:wt32-eth01`). Bukan pengganti checklist operasional di `TODO.md`.

**Production release:** 2026-10-28 · **Doc:** `README.md`, `TODO.md`

---

## Ringkasan

| Aspek                  | Penilaian                                                           |
| ---------------------- | ------------------------------------------------------------------- |
| Struktur & dokumentasi | Siap produksi — README, deploy script, `.env.example`               |
| Loop / opto            | Non-blocking; loop delay default 5 ms                               |
| Web API & validasi     | Server-side checks; cookie `collectHeaders`                         |
| Pusher remote          | `extractPusherCommand` + opto busy guard                            |
| Discord                | Antrean + task terpisah; filter `discord_ev`; tidak block `loop()`  |
| NVS                    | String kosong → `remove`                                            |
| Jaringan               | Wi‑Fi `WIFI_PS_NONE`; ETH opsional — `net_link wifi` jika PHY gagal |
| Performance default    | 160 MHz CPU, 5 ms loop delay (`wt32_config.h`, `apply_env.sh`)      |
| Compile                | OK (~10% flash)                                                     |

**Operasi rekomendasi:** Wi‑Fi + `./apply_env.sh`; Ethernet ditunda jika serial `power up timeout` / `[ETH] begin() failed` (lihat `eth_test/`).

---

## Item review lama — status

| #    | Topik                         | Status                                             |
| ---- | ----------------------------- | -------------------------------------------------- |
| 1    | Pusher JSON parse             | **Selesai** — `wt32_pusher_cmd.cpp`                |
| 2    | NVS empty string              | **Selesai** — `writeStr` → `remove`                |
| 3    | Discord blocking loop         | **Selesai** — `wt32_discord.cpp` worker task       |
| 4    | Static ETH config log         | **Selesai** — log di `wt32_net.cpp`                |
| 5    | Pusher path default           | **Selesai** — `/app/<pusher_key>` jika path kosong |
| 6    | `discord_events` filter       | **Selesai** — `discordAllowsTarget`                |
| 7    | Pusher opto busy              | **Selesai** — log + skip                           |
| 8    | LED GPIO4 contention          | Diterima — factory hold vs press (UX)              |
| 9–12 | Validasi sesi / keamanan HTTP | Dokumentasi README; ganti password default         |

---

## Catatan terbuka (non-blocking release)

- **PHY Ethernet** pada unit tertentu gagal power-up — gunakan **`net_link wifi`**, bukan bug firmware aplikasi.
- **Factory GPIO35** butuh pull-up eksternal (README).
- **HTTP cleartext** — hanya LAN/VLAN tepercaya.

---

## Referensi file

| File                        | Peran                           |
| --------------------------- | ------------------------------- |
| `wt32-eth01-pc-control.ino` | Web, Pusher, session, integrasi |
| `wt32_support.cpp`          | NVS, opto, validasi             |
| `wt32_net.cpp`              | Ethernet + Wi‑Fi                |
| `wt32_discord.cpp`          | Webhook worker                  |
| `wt32_pusher_cmd.cpp`       | Parse payload Pusher            |
| `PusherClient.cpp`          | WebSocket Pusher                |
| `data/js/app.js`            | UI + `formPost`                 |
