# FT232H ↔ ESP32 Ethernet — PC control

Programming lewat **FT232H** (UART), operasi lewat **Ethernet** + **web UI** (HTTP). Kontrol power/restart PC via opto; konfigurasi di NVS.

---

## Ringkasan

| Item             | Nilai                                           |
| :--------------- | :---------------------------------------------- |
| FQBN             | `esp32:esp32:wt32-eth01`                        |
| Serial           | 115200 (`[alive]` + log)                        |
| Web              | `http://<ip>/` — **LittleFS** wajib di-upload   |
| Password default | `12345678`                                      |
| Sesi             | Cookie **session** (tutup browser = login lagi) |

---

## Wiring FT232H (flash / monitor)

| FT232H  | ESP32 |
| :------ | :---- |
| D0 (TX) | RX0   |
| D1 (RX) | TX0   |
| GND     | GND   |

Flash: **GPIO0 → GND**, reset **EN**, upload, lepas GPIO0, reset lagi.

```bash
arduino-cli compile --fqbn esp32:esp32:wt32-eth01 ~/Documents/ywow/arduino/wt32-eth01-pc-control
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:wt32-eth01 ~/Documents/ywow/arduino/wt32-eth01-pc-control
```

---

## Upload UI (LittleFS)

Sumber file UI untuk flash: folder **`data/`**.

**Opsi A — arduino-cli (disarankan jika tersedia):**

```bash
arduino-cli upload-fs -p /dev/ttyUSB0 --fqbn esp32:esp32:wt32-eth01 \
  ~/Documents/ywow/arduino/wt32-eth01-pc-control
```

**Opsi B — script:**

```bash
chmod +x wt32-eth01-pc-control/upload_littlefs.sh
./wt32-eth01-pc-control/upload_littlefs.sh /dev/ttyUSB0
```

Setelah edit tampilan di `data/`, upload LittleFS lagi.

Tanpa LittleFS, serial akan log: `[FS] LittleFS mount failed`.

---

## Penggunaan web

1. Colok **LAN**, tunggu DHCP (LED status nyala saat link+IP).
2. Buka **`http://<ip>/`** → redirect ke login jika belum auth.
3. **Sign in** dengan password (default `12345678`).
4. **Dashboard** — status jaringan/perangkat; field kosong tampil **`()`** (bukan tanda panjang).
5. **PC control** — tombol **Power**, **Power (hold)**, **Restart** (opto).
6. **Settings** — password, theme, network, Pusher, Discord.
7. **System** — **Restart ESP32**; **Clear NVS** (web) atau factory **GPIO35** (hold 5 s).
8. **Sign out** — hapus sesi.

Caption UI: **bahasa Inggris**, font **monospace**.

---

## GPIO

| Fungsi            | GPIO                       |
| :---------------- | :------------------------- |
| Opto power        | 13                         |
| Opto reset        | 14                         |
| LED status        | 4                          |
| Factory NVS clear | 35 (ke GND, pull-up 10 kΩ) |

---

## API HTTP (JSON)

Semua `/api/*` kecuali login butuh cookie sesi.

| Method | Path                 | Fungsi                                   |                    |        |
| :----- | :------------------- | :--------------------------------------- | :----------------- | :----- |
| POST   | `/api/login`         | Password                                 |                    |        |
| POST   | `/api/logout`        | Keluar                                   |                    |        |
| GET    | `/api/session`       | Cek sesi                                 |                    |        |
| GET    | `/api/config`        | Isi form settings                        |                    |        |
| GET    | `/api/status`        | Dashboard                                |                    |        |
| POST   | `/api/theme`         | Theme NVS                                |                    |        |
| POST   | `/api/password`      | Password NVS                             |                    |        |
| POST   | `/api/network`       | Simpan network → reboot                  |                    |        |
| POST   | `/api/network/renew` | Renew DHCP → reboot                      |                    |        |
| POST   | `/api/pusher`        | Simpan + reconnect WS                    |                    |        |
| POST   | `/api/discord`       | Simpan webhook                           |                    |        |
| POST   | `/api/discord/test`  | Tes webhook                              |                    |        |
| POST   | `/api/press`         | Opto (`target=power                      | reset`, `dur=short | long`) |
| POST   | `/api/restart`       | Restart ESP32                            |                    |        |
| POST   | `/api/nvs/clear`     | Hapus semua NVS (`confirm=yes`) → reboot |                    |        |

Static: `/login.html`, `/index.html`, `/css/*`, `/js/*`.

---

## Pusher (remote)

Isi **Pusher / WebSocket** di web (host, port, path, channel). Hanya baris **`press,power,short`** / **`press,reset,...`** dari channel yang dieksekusi (sama spirit `pc_control_v2`).

---

## NVS

- String **kosong tidak disimpan**; key dihapus (`remove`) supaya save tidak gagal (mis. DNS opsional, field Pusher kosong).
- Password web tetap wajib diisi saat **Change password** (min 8 karakter).

---

## Keamanan

- HTTP saja — pakai di LAN terisolasi / VLAN.
- Ganti password default segera.
- Webhook Discord & key Pusher disimpan di NVS — jangan commit dump flash.

---

## File proyek

| File                        | Fungsi                               |
| :-------------------------- | :----------------------------------- |
| `wt32-eth01-pc-control.ino` | ETH, WebServer, API, Pusher, Discord |
| `ft232h_support.cpp/h`      | NVS, validasi, opto non-blocking     |
| `ft232h_config.h`           | Pin & konstanta                      |
| `PusherClient.*`            | WebSocket client                     |
| `data/`                     | UI untuk LittleFS                    |
| `upload_littlefs.sh`        | Upload FS alternatif                 |
| `TODO.md`                   | Checklist & aturan proyek            |

---

## Panduan kode (opsional)

- **Loop** harus ringan: `handleClient`, `optoPoll`, `pusher.loop`, Discord satu POST per antrean.
- **Jangan** `delay()` panjang di handler HTTP (opto pakai state machine).
- Validasi server **selalu**; client JS hanya bantu UX.
- Session: cookie tanpa `Max-Age` panjang — tutup browser = tidak ada cookie.

Lihat juga **`TODO.md`** dan **`AGENT.md`** di root repo.
