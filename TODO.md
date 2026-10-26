# TODO — wt32-eth01-pc-control

## Prinsip

- **HANYA IMPLEMENTASI APA YANG ADA DI TAMPILAN**
- **SELALU CEK & RECHECK** — `arduino-cli compile --fqbn esp32:esp32:wt32-eth01 wt32-eth01-pc-control`
- **PRODUCTION READY** — opto non-blocking, loop ringan, validasi input

## Aturan UI

- [x] Caption **English**, font **monospace**
- [x] Kosong = **`()`** di dashboard (bukan em dash)
- [x] Tombol: **Power**, **Power (hold)**, **Restart**
- [x] Sesi hilang saat tutup browser (cookie session, no Max-Age panjang)
- [x] Validasi client + server

## Backend

- [x] `WebServer` + `Preferences` + `LittleFS`
- [x] API lengkap sesuai mockup
- [x] Opto non-blocking
- [x] Pusher (press only) + Discord webhook + test
- [x] Network NVS + reboot apply
- [x] Theme NVS

## Deploy / QA (manual di hardware)

- [ ] Upload sketch + **upload LittleFS** (`upload_littlefs.sh`)
- [ ] Login, semua form save, press, restart ESP
- [ ] Factory button 5 s
- [ ] Pusher / Discord (jika dipakai)

## Referensi

- `README.md` — cara pakai
- `data/` — UI (LittleFS)
