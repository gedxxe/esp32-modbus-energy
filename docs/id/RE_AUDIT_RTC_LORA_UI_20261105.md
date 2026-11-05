# Audit RTC, LoRa dan panel — 5 November 2026

## RTC: restart sesudah Sync

- Handler HTTP `/api/time` sebelumnya mengalokasikan salinan `Snapshot` besar pada stack `loopTask`. Kini memakai buffer statis, karena WebServer dijalankan secara berurutan di core 0. Ini mengurangi tekanan stack, tetapi belum membuktikan penyebab setiap restart.
- `saveCheckpoint()` memakai buffer statis pada task akuisisi (pemilik tunggal). Watchdog diberi kesempatan di antara pekerjaan NVS, arsip dan capture yang terpisah. Penyimpanan tetap diverifikasi.
- Validasi tanggal, antrean perintah core 1, mutex I2C, penulisan dan baca balik DS3231 serta proteksi rekaman lama tetap dipertahankan.
- Serial Monitor sekarang mengeluarkan `[RTC] sync start`, `[RTC] sync complete`, dan `stack_min`. Bila restart, cocokkan `[Boot] reset reason` serta backtrace. Brownout, WDT, stack overflow, dan koneksi browser terputus adalah kasus yang berbeda.
- Uji DS3231/ESP32-S3 fisik, pergantian hari, dan waktu menjelang 06.00–08.00 / 17.00–19.00 WIB **belum dilakukan**.

## Acrel ADL400

- Profil aktif yang dipilih `acrel-adl400` = `ACREL_ADL400_VERIFIED`. Total energi dibaca FC03 `0x0000` (2 word UInt32 ABCD), skala `/100 × PT × CT` satu kali.
- Raw 9798, PT=1 dan CT=80 menghasilkan 7838,40 kWh. LCD lapangan sebelumnya sekitar 7838,9 kWh; untuk menilai akurasi final, bandingkan pembacaan pada waktu yang sama.
- Profil legacy tidak selectable. ACR dan APM memakai mapping berbeda. Tidak mengubah register, timing serial, faktor CT/PT, atau validasi snapshot.

## LoRa: blocker atau warning?

| Kondisi | Perlakuan |
| --- | --- |
| Enable tanpa key 32 byte / 64 hex | **Blokir Save** sampai key tersedia |
| ID node sama dengan ID gateway | **Blokir Save** |
| Gateway belum ACK | **Peringatan**, Save dan Enable tetap boleh |
| Modul SX1262 tidak merespons | **Peringatan runtime**, Save konfigurasi tetap boleh |
| Radio di-enable ulang dengan key tersimpan | **Boleh**, tidak perlu random key baru |

Key ialah rahasia bersama untuk autentikasi HMAC, bukan hash baru setiap Enable. Generate sekali lalu salin ke konfigurasi gateway; menggantinya pada satu sisi akan memutus autentikasi. Frame pengukuran tidak terenkripsi.

## UI dan README

- Logo Midea vektor biru pada README, logo putih transparan di panel yang tetap tampil melalui AP lokal tanpa internet.
- Copy README awal dan instruksi LoRa/RTC dipangkas agar petunjuk tindakan lebih jelas.
- CI/build tidak menggantikan uji fisik RTC, ADL400 atau LoRa empat node.
