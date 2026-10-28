# TODO — wt32-eth01-pc-control

**Status:** production-ready (2026-10-28) — operasi **Wi‑Fi**; Ethernet opsional (`eth_test/`, `eth0-dhcp.sh`).

## Prinsip

- **HANYA IMPLEMENTASI APA YANG ADA DI TAMPILAN**
- **SELALU CEK & RECHECK** — `arduino-cli compile --fqbn esp32:esp32:wt32-eth01 .`
- **PRODUCTION READY** — opto non-blocking, Discord worker task, loop ringan, validasi input

## Aturan UI

- [x] Caption **English**, font **monospace**
- [x] Kosong = **`()`** di dashboard (bukan em dash)
- [x] Tombol: **Power**, **Power (hold)**, **Restart**
- [x] Sesi hilang saat tutup browser (cookie session, no Max-Age panjang)
- [x] Validasi client + server
- [x] Network: **auto / ethernet / wifi** + Wi‑Fi SSID/pass di form
- [x] POST form **application/x-www-form-urlencoded** (`formPost`)

## Backend

- [x] `WebServer` + `Preferences` + `LittleFS`
- [x] API lengkap sesuai mockup
- [x] Opto non-blocking
- [x] Pusher JSON + plain `press,...` (`wt32_pusher_cmd`)
- [x] Discord webhook (queue + FreeRTOS task, filter `discord_ev`)
- [x] Network NVS + reboot apply
- [x] Theme NVS
- [x] **Ethernet + Wi‑Fi** (`wt32_net`); Wi‑Fi `WIFI_PS_NONE` untuk latency
- [x] **Serial NVS** (`help`, `set`, `show`, `reboot`)
- [x] Performance default **160 MHz**, **5 ms** loop delay

## Dokumentasi

- [x] **`README.md`** — flash, jaringan, serial, web, Pusher, Discord, production deploy
- [x] **`REVISION.md`** — riwayat review + item selesai
- [x] **`deploy.sh`** — compile + flash + FS + `apply_env.sh`

## Deploy production (hardware)

- [x] `./deploy.sh` atau langkah manual di README
- [x] **`NET_LINK=wifi`** + SSID di `.env` jika PHY Ethernet tidak dipakai
- [ ] Ganti **password web** default setelah login pertama
- [ ] Ethernet (jika PHY OK): DHCP router → `[ETH] IP`

## Referensi

- `README.md` — cara pakai & koneksi jaringan
- `data/` — UI (LittleFS)
