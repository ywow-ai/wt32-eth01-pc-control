# TODO — wt32-eth01-pc-control

## Prinsip

- **HANYA IMPLEMENTASI APA YANG ADA DI TAMPILAN**
- **SELALU CEK & RECHECK** — `arduino-cli compile --fqbn esp32:esp32:wt32-eth01 .`
- **PRODUCTION READY** — opto non-blocking, loop ringan, validasi input

## Aturan UI

- [x] Caption **English**, font **monospace**
- [x] Kosong = **`()`** di dashboard (bukan em dash)
- [x] Tombol: **Power**, **Power (hold)**, **Restart**
- [x] Sesi hilang saat tutup browser (cookie session, no Max-Age panjang)
- [x] Validasi client + server
- [x] Network: **auto / ethernet / wifi** + Wi‑Fi SSID/pass di form

## Backend

- [x] `WebServer` + `Preferences` + `LittleFS`
- [x] API lengkap sesuai mockup
- [x] Opto non-blocking
- [x] Pusher (press only) + Discord webhook + test
- [x] Network NVS + reboot apply
- [x] Theme NVS
- [x] **Ethernet + Wi‑Fi** (`wt32_net`)
- [x] **Serial NVS** (`help`, `set`, `show`, `reboot`)

## Dokumentasi

- [x] **`README.md`** — tutorial flash, jaringan (LAN/WLAN/direct), serial, web

## Deploy / QA (manual di hardware)

- [ ] Upload sketch + **upload LittleFS** (`upload_littlefs.sh`)
- [ ] **Wi‑Fi only:** `set net_link wifi` + SSID → `[WIFI] IP` → web login
- [ ] **Ethernet** (jika PHY OK): DHCP ke router → `[ETH] IP`
- [ ] Login, semua form save, press, restart ESP
- [ ] Factory button 5 s
- [ ] Pusher / Discord (jika dipakai)

## Referensi

- `README.md` — cara pakai & koneksi jaringan
- `data/` — UI (LittleFS)
