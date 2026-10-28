# WT32-ETH01 PC control (web + opto)

Kontrol power/restart PC via opto; konfigurasi lewat **web**, **serial**, atau **NVS**. Jaringan: **Ethernet (LAN)**, **Wi‑Fi (WLAN)**, atau **keduanya (auto)**.

Programming: **FT232H** (UART) atau adapter USB‑serial 3,3 V. Operasi: **HTTP** di `http://<ip>/`.

---

## Ringkasan

| Item             | Nilai                                            |
| :--------------- | :----------------------------------------------- |
| FQBN             | `esp32:esp32:wt32-eth01`                         |
| Serial           | **115200** — log, perintah NVS (`help`)          |
| Web              | `http://<ip>/` — **LittleFS** wajib di-upload    |
| Password default | `12345678`                                       |
| Sesi             | Cookie **session** (tutup browser = login lagi)  |
| Jaringan default | `net_link=auto` (Ethernet + Wi‑Fi jika SSID ada) |

---

## Tutorial 1 — Flash firmware & UI

### 1.1 Wiring FT232H (upload / monitor)

| FT232H         | WT32 (header)                                      |
| :------------- | :------------------------------------------------- |
| D0 (TX)        | **RX0**                                            |
| D1 (RX)        | **TX0**                                            |
| GND            | **GND**                                            |
| 5 V (opsional) | **5 V** — atau daya terpisah; **GND harus common** |

**Upload:** **GPIO0 → GND** → tekan **EN** → upload → **lepas GPIO0** → **EN** lagi.

```bash
arduino-cli compile --fqbn esp32:esp32:wt32-eth01 \
  ~/Documents/ywow/arduino/wt32-eth01-pc-control

arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:wt32-eth01 \
  ~/Documents/ywow/arduino/wt32-eth01-pc-control
```

### 1.2 Upload LittleFS (web UI)

Tanpa langkah ini serial menampilkan: `[FS] LittleFS mount failed`.

```bash
arduino-cli upload-fs -p /dev/ttyUSB0 --fqbn esp32:esp32:wt32-eth01 \
  ~/Documents/ywow/arduino/wt32-eth01-pc-control
```

Atau:

```bash
chmod +x upload_littlefs.sh
./upload_littlefs.sh /dev/ttyUSB0
```

### 1.3 Cek boot di serial

Setelah reset, harus muncul (contoh):

- `[BOOT] pc_control LAN/WLAN ...`
- `[SERIAL] NVS: type 'help' for commands`
- `[WEB] HTTP :80 ...`
- `[ETH] IP ...` dan/atau `[WIFI] IP ...` jika jaringan OK
- Tiap ~5 s: `[alive] http://<ip>/ (ethernet|wifi)`

---

## Tutorial 2 — Menghubungkan ke jaringan (lengkap)

Semua pengaturan jaringan disimpan di **NVS**. Setelah ubah **link** atau IPv4 lewat web → **reboot**. Lewat serial → **`reboot`** setelah `set ...`.

### 2.1 Mode koneksi (`net_link`)

| Nilai        | Arti                                                                                   |
| :----------- | :------------------------------------------------------------------------------------- |
| **auto**     | Nyalakan **Ethernet** dan **Wi‑Fi** (STA). Web/API hidup jika **salah satu** dapat IP. |
| **ethernet** | **Hanya LAN** (WT32 RJ45). Wi‑Fi tidak di-start.                                       |
| **wifi**     | **Hanya Wi‑Fi**. `ETH.begin()` tidak dipanggil — cocok jika PHY Ethernet bermasalah.   |

Set lewat web: **Settings → Network → Connection**.  
Set lewat serial:

```text
set net_link auto
set net_link ethernet
set net_link wifi
reboot
```

### 2.2 Ethernet ke router / switch (DHCP) — paling umum

1. **`net_link`:** `auto` atau `ethernet`.
2. **`ip_mode`:** `dhcp` (default).
3. Colok **kabel LAN** modul ke **port LAN router** (bukan port WAN ISP saja — harus satu LAN yang sama dengan PC/laptop jika tes lokal).
4. Reset **EN**. Serial: `[ETH] IP 192.168.x.x`.
5. Browser: `http://192.168.x.x/` → login.

**LED status (GPIO4):** nyala saat **ada IP** (Ethernet atau Wi‑Fi).

### 2.3 Ethernet IP statis

1. Web **Settings → Network** (atau serial):

   ```text
   set net_link ethernet
   set ip_mode static
   set ip_addr 192.168.1.50
   set subnet 255.255.255.0
   set gateway 192.168.1.1
   set dns1 8.8.8.8
   set dns2
   reboot
   ```

2. Pastikan gateway benar dan tidak bentrok dengan DHCP router.

### 2.4 Wi‑Fi ke router (WLAN)

1. **`net_link`:** `wifi` atau `auto`.
2. Isi SSID & password (web atau serial):

   ```text
   set net_link wifi
   set wifi_ssid NamaWiFi
   set wifi_pass SandiWiFi
   set ip_mode dhcp
   reboot
   ```

3. Serial: `[WIFI] IP ...` → buka `http://<ip-wifi>/`.

**Catatan:** WT32 punya **antena Wi‑Fi internal**; jangkau dan stabilitas tergantung router.

### 2.5 Auto — LAN + Wi‑Fi

- **`net_link auto`** + SSID terisi → keduanya dicoba.
- Dashboard menampilkan **Ethernet IP** dan **Wi‑Fi IP** jika keduanya hidup.
- `[alive]` di serial menampilkan IP utama; jika keduanya up, baris tambahan untuk Wi‑Fi.

### 2.6 Kabel langsung modul ↔ laptop (tanpa router)

Untuk tes DHCP dari laptop:

1. **Jangan** colok `eth0` laptop ke router saat skrip ini jalan.
2. Di laptop (Linux):

   ```bash
   sudo ./eth0-dhcp.sh
   ```

   `eth0` menjadi **192.168.99.1/24**, DHCP **192.168.99.50–150**.

3. Modul: `net_link ethernet`, `ip_mode dhcp`, kabel LAN ke **eth0** laptop.
4. Reset modul → serial `[ETH] IP 192.168.99.x` → browser `http://192.168.99.x/`.

### 2.7 Troubleshooting jaringan

| Gejala serial                               | Langkah                                                                                    |
| :------------------------------------------ | :----------------------------------------------------------------------------------------- |
| `power up timeout` / `[ETH] begin() failed` | PHY/LAN mati → pakai **`set net_link wifi`** + SSID, atau ganti board / mod hardware WT32. |
| `[WIFI] no SSID`                            | `set wifi_ssid ...` lalu `reboot`.                                                         |
| Web tidak buka, `[alive]` ada               | Pastikan PC **satu subnet** dengan IP modul; cek firewall.                                 |
| LittleFS gagal                              | Upload **`data/`** lagi (GPIO0 saat upload FS).                                            |

---

## Tutorial 3 — Konfigurasi lewat serial (NVS)

Monitor **115200**, akhiri baris dengan **Enter**.

| Perintah                | Fungsi                             |
| :---------------------- | :--------------------------------- |
| `help`                  | Daftar perintah & key              |
| `show`                  | Dump NVS (password/Wi‑Fi di-mask)  |
| `show secrets`          | Tampilkan `wifi_pass` & `web_pass` |
| `get <key>`             | Satu key NVS (secrets di-mask)     |
| `set <key> <value>`     | Simpan satu key ke NVS             |
| `reboot` / `restart`    | Reset ESP32                        |
| `nvs clear confirm=yes` | Factory NVS + reboot               |

**Key yang bisa di-set:**

`net_link`, `wifi_ssid`, `wifi_pass`, `ip_mode`, `ip_addr`, `subnet`, `gateway`, `dns1`, `dns2`, `theme_mode`, `web_pass` (atau `password`), `pusher_chan`, `ws_host`, `ws_port`, `ws_path`, `pusher_key`, `pusher_cluster`, `discord_en`, `discord_url`, `discord_user`, `discord_ev`, `cpu_mhz`, `loop_delay_ms`

Contoh ganti password web:

```text
set web_pass rahasiaBaru123
```

Pusher: setelah `set ws_host ...` dll., firmware **reconnect Pusher** otomatis (tanpa reboot), kecuali perubahan jaringan.

---

## Tutorial 4 — Penggunaan web

1. Pastikan **`[alive]`** atau Dashboard ada IP.
2. Buka **`http://<ip>/`** → **Sign in** (default `12345678`).
3. **Dashboard** — link, IP Ethernet/Wi‑Fi, gateway, MAC, uptime.
4. **PC control** — **Power**, **Power (hold)**, **Restart** (opto GPIO13/14).
5. **Settings**
   - **Password** — min 8 karakter.
   - **Appearance** — theme.
   - **Network** — `net_link`, Wi‑Fi, IPv4 → **Save network** → reboot.
   - **Pusher / Discord** — remote & notifikasi.
6. **System** — restart ESP; Clear NVS (web). **Factory:** GPIO35 ke GND **~5 s** (lepas dulu, lalu hold).

UI: caption **English**, font **monospace**; field kosong = **`()`**.

---

## GPIO

| Fungsi            | GPIO                       |
| :---------------- | :------------------------- |
| Opto power        | 13                         |
| Opto reset        | 14                         |
| LED status        | 4                          |
| Factory NVS clear | 35 (ke GND, pull-up 10 kΩ) |

**Jangan** tarik **GPIO0** ke GND saat operasi normal (hanya saat flash). **IO0** = clock Ethernet.

---

## API HTTP (JSON)

Semua `/api/*` butuh cookie sesi, **kecuali** `POST /api/login` dan `GET /api/session`.

| Method | Path                 | Fungsi                                                |
| :----- | :------------------- | :---------------------------------------------------- |
| POST   | `/api/login`         | Password                                              |
| POST   | `/api/logout`        | Keluar                                                |
| GET    | `/api/session`       | Cek sesi                                              |
| GET    | `/api/config`        | Isi form settings                                     |
| GET    | `/api/status`        | Dashboard (termasuk `eth_ip`, `wifi_ip`, `active_if`) |
| POST   | `/api/theme`         | Theme NVS                                             |
| POST   | `/api/password`      | Password NVS                                          |
| POST   | `/api/network`       | Simpan network → reboot                               |
| POST   | `/api/network/renew` | Renew DHCP → reboot                                   |
| POST   | `/api/pusher`        | Simpan + reconnect WS                                 |
| POST   | `/api/discord`       | Simpan webhook                                        |
| POST   | `/api/discord/test`  | Tes webhook (queued)                                  |
| POST   | `/api/press`         | Opto (`target=power\|reset`, `dur=short\|long`)       |
| POST   | `/api/performance`   | CPU MHz + loop delay                                  |
| POST   | `/api/restart`       | Restart ESP32                                         |
| POST   | `/api/nvs/clear`     | Hapus NVS (`confirm=yes`) → reboot                    |

Static: `/login.html`, `/index.html`, `/css/*`, `/js/*`.

---

## Pusher (remote internet)

Isi **Pusher / WebSocket** di web. Perintah channel: **`press,power,short`** / **`press,reset,...`**.

Tes dari PC: `.env.example` → `.env`, lalu `./push_pusher.sh`. Kredensial ESP tetap di **NVS** (web/serial).

---

## Discord

Webhook lewat **antrean + task** (non-blocking di `loop()`).

---

## NVS

- String kosong → key **dihapus** (`remove`).
- Performance default: **80 MHz**, **10 ms** loop delay.
- `wifi_pass` / `web_pass` di serial `show` default **masked**.

---

## Keamanan

- HTTP tanpa TLS — gunakan **LAN/VLAN tepercaya** atau VPN.
- Ganti password default segera.
- Jangan commit `.env` / dump flash berisi webhook.

---

## File proyek

| File                                   | Fungsi                           |
| :------------------------------------- | :------------------------------- |
| `wt32-eth01-pc-control.ino`            | WebServer, API, integrasi        |
| `wt32_net.cpp/h`                       | Ethernet + Wi‑Fi, status IP      |
| `wt32_serial_cmd.cpp/h`                | Perintah serial NVS              |
| `wt32_support.cpp/h`                   | NVS, validasi, opto              |
| `wt32_pusher_cmd.*` / `PusherClient.*` | Pusher                           |
| `wt32_discord.*`                       | Discord worker                   |
| `wt32_config.h`                        | Pin & konstanta                  |
| `data/`                                | UI LittleFS                      |
| `upload_littlefs.sh`                   | Upload FS                        |
| `eth0-dhcp.sh`                         | DHCP laptop ↔ modul (tes direct) |
| `eth_test/`                            | Sketch tes PHY Ethernet saja     |

---

## Build

```bash
arduino-cli compile --fqbn esp32:esp32:wt32-eth01 \
  ~/Documents/ywow/arduino/wt32-eth01-pc-control
```

Lihat **`TODO.md`** (checklist QA) dan **`REVISION.md`** (catatan review kode).
