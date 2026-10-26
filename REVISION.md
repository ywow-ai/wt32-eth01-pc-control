# REVISION — wt32-eth01-pc-control

Tinjauan kode (review statis + compile `esp32:esp32:wt32-eth01`). Bukan pengganti QA hardware di `TODO.md`.

**Tanggal review:** 2026-10-26

---

## Ringkasan

| Aspek                      | Penilaian                                      |
| -------------------------- | ---------------------------------------------- |
| Struktur & dokumentasi     | Baik — README/TODO selaras dengan implementasi |
| Loop / opto (jalur normal) | Baik — state machine non-blocking              |
| Web API & validasi         | Cukup baik — server-side checks ada            |
| Pusher remote              | **Diperbaiki** — `wt32_pusher_cmd` + path key  |
| NVS save field kosong      | **Diperbaiki** — empty → `remove` key          |
| Discord webhook            | **Dimitigasi** — kirim saat opto idle          |
| Compile                    | OK (~10% flash)                                |

Kesimpulan: siap QA hardware setelah password diganti, upload sketch + LittleFS; Discord test masih blocking singkat saat opto idle.

---

## Prioritas tinggi

### 1. Pusher: payload JSON tidak diparse

**Lokasi:** `wt32-eth01-pc-control.ino` — `pusherOnMessage()`

Handler hanya menerima string yang **langsung** diawali `press,`. Server Pusher mengirim frame JSON (mis. `client-event` / `data` berisi `{"cmd":"press,power,short"}`), sehingga perintah remote **biasanya diabaikan** meskipun WebSocket connect dan subscribe sukses.

**Referensi:** `pc_control_v2/pc_control_v2.ino` — `extractPusherCommand()`, `onPusherMessage()`.

**Rekomendasi:** Port logic extract + sanitize cmd dari v2; terima fallback plain text `press,...` untuk proxy/custom WS.

---

### 2. NVS: `writeStr()` gagal pada string kosong

**Lokasi:** `ft232h_support.cpp` — `writeStr()`

```cpp
return g_prefs.putString(key, val) > 0;
```

Di ESP32, `putString("")` sering mengembalikan `0` → save dianggap gagal. `nvsSaveNetwork()`, `nvsSavePusher()`, dan sebagian field Discord memakai rantai `&&` — **satu field kosong (DNS opsional, host/port kosong, dll.) bisa membatalkan seluruh save**.

**Referensi:** `pc_control_v2` — empty value → `prefs.remove(key)` atau dianggap OK.

**Rekomendasi:** Untuk `val.isEmpty()`, hapus key atau anggap sukses tanpa `putString`.

---

### 3. Discord: HTTP blocking mengganggu opto

**Lokasi:** `wt32-eth01-pc-control.ino` — `pollDiscord()`, `handleApiDiscordTest()`, `sendDiscordWebhook()`

`HTTPClient` dengan timeout 4000 ms dipanggil di `loop()` dan di handler API. Selama blocking, **`optoPoll()` tidak berjalan** → GPIO opto bisa tetap HIGH jauh lebih lama dari `PULSE_SHORT_MS` / `PULSE_LONG_MS`.

Bentrok dengan prinsip di README: loop ringan, tidak `delay()` panjang di jalur kritis (opto harus di-poll teratur).

**Rekomendasi:** Antre pesan saja; kirim webhook hanya saat opto idle, atau state machine HTTP non-blocking; jangan POST di dalam handler `/api/discord/test` tanpa batas waktu ketat / async.

---

### 4. Static IP: kegagalan `ETH.config` diabaikan

**Lokasi:** `wt32-eth01-pc-control.ino` — `setup()`

`applyStaticEthConfig()` return value tidak dicek sebelum `ETH.begin()`. NVS mode `static` dengan IP/gateway/subnet invalid → boot tetap lanjut, perilaku jaringan bisa **tidak match** UI/NVS tanpa log jelas.

**Rekomendasi:** Log error + fallback DHCP atau tolak save di `nvsSaveNetwork` (sudah divalidasi saat save; masalah mainly jika NVS corrupt/manual).

---

## Prioritas sedang

### 5. Field Pusher `pusher_key` / `pusher_cluster` tidak dipakai saat connect

Disimpan NVS dan ditampilkan di `/api/config`, tetapi `pusherReconnect()` hanya memakai `ws_host`, `ws_port`, `ws_path`, `pusher_chan`. Untuk Pusher resmi, path biasanya `/app/<APP_KEY>` — user harus isi manual di **ws_path**; key/cluster di form menyesatkan jika diisi tanpa path.

**Rekomendasi:** Dokumentasi di UI/README, atau bangun default path dari key + cluster saat path kosong.

---

### 6. `discord_events` tidak difilter

Disimpan (`discord_ev`) tetapi `queueDiscord()` mengirim semua event (web press, Pusher) jika Discord enabled — field “Events (comma-separated)” **tidak berpengaruh**.

**Rekomendasi:** Implement filter sesuai mockup, atau hapus field dari UI sampai dipakai.

---

### 7. Pusher press tanpa `optoBusy()`

API web mengembalikan `409 Opto busy`; `pusherOnMessage()` memanggil `optoRequest()` yang **silent drop** jika busy. Perintah kedua hilang tanpa log Discord/serial.

**Rekomendasi:** Log `[Pusher] opto busy` atau antre (hati-hati antre untuk power button).

---

### 8. LED status GPIO4 — kontention

`pollFactoryButton()` (blink saat hold factory), `ledPulsePoll()` (feedback press), dan event ETH (`g_ethHasIp`) semua menggerakkan `PIN_LED_STATUS`. Kombinasi hold factory + press opto bisa tampilan LED aneh (bukan opto salah, tapi UX/debug).

---

## Prioritas rendah / desain

### 9. Validasi IPv4 server vs client

Server: `sscanf` — client JS: tolak leading zero (`192.168.01.1`). Inkonsistensi kecil; edge case jarang.

### 10. Sesi web

- Satu `g_sessionId` di RAM; login baru mengganti sesi lama; reboot ESP = logout semua.
- Cookie `sid=...` tanpa `Max-Age` panjang = session cookie browser (sesuai README).
- Parsing `Cookie` via `indexOf("sid=" + id)` — cukup untuk perangkat tunggal di LAN.

### 11. Keamanan operasional

HTTP cleartext, password default `12345678`, webhook/key di NVS — sudah di README; bukan bug kode, risiko di LAN lebar / VLAN salah.

### 12. Factory reset GPIO35

`INPUT` tanpa pull internal; README mensyaratkan pull-up eksternal. Pin floating → risiko false trigger (hardware).

---

## Yang sudah solid

- Opto: `optoInit` / `optoRequest` / `optoPoll` / `optoBusy` di API web.
- `jsonEscape` pada JSON config/status.
- Validasi: theme, ip mode, channel name, port, Discord URL (saat enabled).
- `PusherClient`: ping/pong, subscribe, reconnect interval — selaras `pc_control_v2`.
- LittleFS static + redirect login untuk `/` dan `/index.html`.
- UI (`data/js/app.js`): validasi IPv4/port, `credentials: "same-origin"`.

---

## Checklist perbaikan (usulan)

- [x] Port `extractPusherCommand` (`wt32_pusher_cmd.cpp`)
- [x] Perbaiki `writeStr` untuk nilai kosong
- [x] Discord: `pollDiscord` skip saat opto busy; test 409 jika busy
- [x] Log `applyStaticEthConfig()` gagal di boot
- [x] Default path `/app/<key>` + hint UI
- [x] Filter `discord_events` by target (power/reset)
- [x] CSS `.msg` / `.hidden`; caption network reboot
- [ ] QA manual: `TODO.md` setelah patch

---

## Referensi file

| File                              | Peran review                               |
| --------------------------------- | ------------------------------------------ |
| `wt32-eth01-pc-control.ino`       | Web, Pusher handler, Discord, ETH, session |
| `wt32_support.cpp`                | NVS, validasi, opto, LED pulse             |
| `wt32_pusher_cmd.cpp`             | Parse payload Pusher / plain `press,...`   |
| `PusherClient.cpp`                | Protokol WS Pusher (bukan parsing cmd)     |
| `pc_control_v2/pc_control_v2.ino` | Referensi Pusher + NVS empty string        |
