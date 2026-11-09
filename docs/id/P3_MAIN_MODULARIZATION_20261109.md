# P3 — Refactoring main.cpp secara inkremental (2026-11-09)

## Ruang lingkup

Baseline awal: sesudah PR #27 (`109946cd4b748817770019ebc4fdc9808dfca300`). **Disinkronkan kembali dengan `main` sesudah merge PR #29** (commit `d79a202d0a12c5ac7edcc2a44e9f80fe8830e7a1`). **Tidak mengubah framework**, versi SDK, toolchain, dependency, isi ESP32 IoT cloud payload, jadwal pencatatan, endpoint atau website.

## Pemisahan yang selesai di PR ini

### 1. `bounded_web_server.h/.cpp`

Pindahkan subclass WebServer yang sebelumnya berada di `main.cpp` menjadi modul HTTP transport:

- Kebijakan lama `HC_WAIT_READ` pada speculative socket 250ms tetap dipertahankan.
- `WebServer::handleClient()` tetap dipanggil seperti semula dengan mode synchronous.
- Bounded `send(MSG_DONTWAIT)` mempertahankan budget **5000ms dari awal request**, chunk maksimal 1024 byte, koneksi diputus jika write tidak komplet.
- Callback idle/watchdog/penggerak LED masuk melalui dependency injection `void (*idlePump)()` dari UI core. Modul tidak memiliki state meter, WiFi Enterprise, LoRa, IoT atau RTC.
- Tidak ada perubahan pada validasi autentikasi HTTP, WiFi, atau parsing request.

### 2. `iot_status_web.h/.cpp`

Memindahkan serialisasi **51 field** `/api/status.iot` ke modul tanpa state. API `addIoTStatusJSON(JsonObject,iot::Status,nowMs)` menerima snapshot status worker dan timestamp dari pemanggil. Semua nama/tipe field, `workerPulseAgeMs`, `lastAckSlot=-1` saat hari ACK belum ada dan indikator `backfill` dari PR #27 dipertahankan.

`main.cpp` tetap pemilik server route, network state, queues, akuisisi meter, RTC, storage dan task scheduling. Perubahan ini **bukan** klaim `main.cpp` sudah selesai dimodularisasi; tujuan utamanya memisahkan transport dan presentasi status sebelum memindahkan handlers lain.

### 3. Regression gate

`tools/test_iot_status_schema.py` membekukan daftar 51 field status, mengecek bahwa hanya satu titik pemanggilan serializer pada `main.cpp`, dan mengunci integrasi `addLoRaJSON()` dari PR #29: field `irqNotifications` / `irqPinFallbacks` serta pola notifikasi ISR FreeRTOS tidak boleh hilang. Test juga memeriksa pin **pioarduino platform 55.03.311 / Core 6.1.19**, agar refactoring tidak meng-upgrade framework. Ini **static contract test**, bukan simulasi ESP32. Native test `boundedWrite` yang lama, browser regression dan full ESP32-S3 build tetap wajib.

### 4. Reaudit kompatibilitas pasca merge PR #29

- PR #29 mengubah `firmware/include/lora_irq_policy.h`, `lora_service.h`, `firmware/src/lora_service.cpp`, `lora_web.cpp`, `firmware/test/test_power_lora.cpp` serta dokumentasi P2. PR #30 tidak mengubah satu pun file itu: **nol tumpang tindih path**, sehingga merge branch dapat mempertahankan seluruh blob LoRa persis seperti `main`.
- Sinkronisasi dilakukan dengan **merge commit dua-parent** (branch PR #30 + `main` hasil PR #29), bukan menyalin ulang versi LoRa lama. Perbedaan `main...PR30` kembali tinggal delapan file P3. Kontrak endpoint `d["lora"]` tetap dipanggil via `addLoRaJSON()` sebelum `d["iot"]` didelegasikan ke `addIoTStatusJSON()`.
- Perubahan LoRa ISR tetap milik task `sx1262-link`; pemisahan HTTP tidak memindahkan thread, callback ISR, antrean, SPI, atau pemrosesan HMAC. Status IRQ baru otomatis tersedia dari serializer LoRa yang sudah ter-merge.
- Verifikasi GitHub menyatakan PR **mergeable**, tetapi **kompatibilitas operasional baru dapat diklaim setelah CI gabungan + staging**; status CI harus direkam pada commit terbaru.
- Tidak ada modifikasi pada `firmware/platformio.ini`, `requirements-dev.txt`, source framework, skema penyimpanan NVS atau repository website.

## Batas keamanan

Versi WebServer masih Arduino-ESP32 3.3.11. Di upstream `libraries/WebServer/src/Parsing.cpp`, request line dan header dibaca memakai `client.readStringUntil()` yang dapat menunggu berkali-kali. Transport adapter hasil extraction **tidak** memperbaiki slow-header hang. Jangan menjual refactor ini sebagai pengganti upgrade/patched parser. Review patch lokal atau admission pre-parser yang teruji dengan request parsial merupakan pekerjaan keamanan lanjutan; aturan pelanggan saat ini melarang pergantian framework.

## Fase modularisasi lanjutan (belum diterapkan)

1. Ekstrak `/api/uart` serta `/api/trace` ke serializer terpisah, setelah memiliki fixture JSON field dan snapshot yang konsisten.
2. Pisahkan WiFi scan lifecycle setelah membuat mocked event-sequence harness (termasuk same SSID stale callbacks).
3. Pecah route registration `/api/network` menjadi handler terisolasi hanya jika command/core queue admission dan penyimpanan CA bisa dipertahankan atomik.
4. Buat antarmuka `Snapshot` typed di modul sendiri, *kemudian* pecah acquisition/storage; jangan langsung memindahkan globals lintas core tanpa audit ownership.
5. Staging 24/7 yang mencakup HTTP slow clients, heap, stack high watermark, watchdog, Enterprise handoffs, HTTPS live/heartbeat + 30-day archive.

## P0 yang tetap perlu validasi

- Nama sertifikat RADIUS dan CA diverifikasi IT, migrasi legacy PEAP dari PR #27.
- HTTPS staging dengan receipt/ACK backend yang benar dan kegagalan DHCP/DNS/sockets.
- WebServer 3.3.11 slow-header denial-of-service belum ditutup oleh ekstraksi modul; tidak ada perubahan version pin.

**Tidak boleh merge otomatis**. Hanya source firmware dan regression tests yang diubah.
