# P2 — Audit sinkronisasi interrupt dan task LoRa (2026-11-09)

## Baseline dan keputusan versi

Baseline firmware adalah `main` sesudah PR #27 (`109946cd4b748817770019ebc4fdc9808dfca300`). PR #28 ditolak dan **ditutup tanpa merge**. Jangan upgrade framework atau dependency apa pun dalam PR ini. `firmware/platformio.ini`, `requirements-dev.txt`, endpoint IoT, payload LoRa, dan repositori website tetap sama.

## Temuan high severity — komunikasi ISR/task

Pada `firmware/src/lora_service.cpp`, callback DIO1 radio sebelumnya hanya menjalankan `volatile bool irqPending=true`. Task radio membaca `irqPending` kemudian mengembalikannya menjadi false. **volatile bukan primitif sinkronisasi RTOS**; ISR dapat menulis lagi di antara pembacaan dan pembersihan, menyebabkan notifikasi baru hilang. Walau pemeriksaan level DIO1 lama dapat menyelamatkan sebagian kasus, tidak ada jaminan seluruh edge mendapat pemrosesan terjadwal. Ini risiko yang dapat dibuktikan dari pola kode, **bukan klaim packet loss lapangan yang sudah direproduksi**.

## Perubahan implementasi

1. ISR hanya memanggil `vTaskNotifyGiveFromISR(worker,&higherPriorityWoken)`, serta `portYIELD_FROM_ISR()` bila perlu; tidak menjalankan `radio.getIrqFlags`, SPI, autentikasi HMAC, logging, atau alokasi memori.
2. Task radio mengambil notifikasi dengan `ulTaskNotifyTake(pdTRUE,0)`. Jumlah notifikasi dapat digabung; satu pembacaan flag radio digunakan untuk status IRQ yang tertahan.
3. Pemeriksaan `digitalRead(LORA_DIO1)==HIGH` tetap sebagai fallback bila edge terjadi sebelum callback diarm atau notifikasi tidak teramati. Helper murni `lora_irq_policy.h` diuji native.
4. Setelah inisialisasi ulang sesi radio, notifikasi lama dibersihkan **sebelum** memasang callback lagi, agar konfigurasi lama tidak memicu status palsu. IRQ yang masih HIGH tetap dapat diamati.
5. Tambahkan counter diagnostik `irqNotifications` dan `irqPinFallbacks` pada objek `lora` di status web, tanpa mengubah format frame, session/ACK validation, polling, interval, duty gate, retry, atau konfigurasi SX1262.
6. Seluruh kode protokol, penerimaan, CAD, TX dan ACK tetap dimiliki task `sx1262-link` pada core 0. `main.cpp` tidak dipindahkan dalam PR ini.

**Sifat notifikasi:** notifikasi RTOS dapat bergabung/coalesce. Counter `irqNotifications` adalah total notifikasi yang dikonsumsi, bukan jumlah paket atau jumlah pasti transisi fisik. Counter `irqPinFallbacks` adalah kesempatan task melihat pin HIGH tanpa notifikasi yang masih terpending, bukan bukti IRQ yang hilang.

## Acceptance

- Wajib CI: `firmware/test/test_power_lora.cpp` (termasuk tabel edge/level dan semua fixture LoRa v1), native suite, `tools/check_lora.py`, build ESP32-S3 N16R8 pada core yang sama, synthetic HTTPS build dan browser regression.
- Setelah CI hijau, wajib sesi staging: (a) RX setelah boot dan init ulang, (b) TX/CAD/RX bergantian, (c) ACK cepat, terlambat dan duplikat, (d) retry dan kondisi channel sibuk, (e) hilangnya koneksi, (f) rotasi session serta update LoRa settings ketika RX sedang aktif.
- Pantau `irqNotifications`, `irqPinFallbacks`, `crcErrors`, `timeouts`, `replayErrors`, `cadErrors`, `tx`, `rx` selama uji. Tidak ada klaim validasi 24/7 tanpa uji tersebut.
- Pemeriksaan stack watermark task LoRa dan task watchdog tetap diperlukan.

## Keterbatasan dan risiko lain

- Tidak membuktikan tidak ada race antara callback RadioLib yang tidak tercakup oleh semantik driver; runtime RadioLib tetap perlu staging.
- Tidak mengubah urutan prioritas status `CAD_DONE`/`TX_DONE`/`RX_DONE` atau aturan kontensi: perubahan tersebut membutuhkan pengujian frame-level dan gateway yang nyata.
- Potensi ISR spuriously notifying saat state inisialisasi dapat menyebabkan status flag kosong diproses; task tetap memakai fallback existing `receiveMode()` dan retry. Uji staging wajib.
- Tidak ada perubahan besar pada FreeRTOS task priority maupun polling interval 10ms.

## P0 yang masih terbuka

- PR #27 menambah penegakan nama RADIUS untuk **login PEAP baru**, tetapi profil lama tanpa nama tetap bisa retry. Butuh nama sertifikat server sah dari IT, migrasi terkontrol dan uji PEAP real (CA + waktu + DHCP).
- Validasi end-to-end HTTPS ke website staging masih belum tercatat dengan token tes yang valid; jangan mengklaim penerimaan ACK langsung.
- Pada Arduino-ESP32 3.3.11, `WebServer::handleClient()` dapat memanggil `_parseRequest()` yang memakai `readStringUntil` per header dalam loop tanpa deadline menyeluruh. Pembatas waktu **write** yang ada tidak membatasi **parse**. PR #28 dibatalkan sesuai keputusan owner. Tanpa framework upgrade, solusi penuh membutuhkan perbaikan parser spesifik teruji/isolasi layanan yang diaudit terpisah. Pengukuran setelah pemanggilan `handleClient()` tidak menghentikan parser yang sedang blocking. Hindari endpoint web tanpa pembatasan akses jaringan sampai ada workaround yang dibuktikan lewat test.

Tidak ada perubahan hardware, metode Modbus, atau automatic merge.
