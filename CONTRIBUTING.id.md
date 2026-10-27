# Kontribusi

[English](CONTRIBUTING.md) · **Bahasa Indonesia**

Baca [AGENTS.md](AGENTS.md) sebelum mengubah firmware. UART, Preferences dan tracker hanya dimiliki task akuisisi core 1. UI mengirim nilai perintah melalui queue. Jangan menambah write register meter atau menggeser register untuk mengatasi masalah wiring.

## Lingkungan dan pemeriksaan

PlatformIO Core 6.1.16, g++ C++11 dan Python 3.10+ diperlukan untuk firmware/tes. Source browser memakai Node **24.22.1** dan `npm ci`. Build PlatformIO biasa tidak memerlukan Node karena header web sudah disertakan dan diverifikasi hash.

```powershell
.\tools\test.ps1
python tools/generate_docs.py
python tools/check_docs.py
npm run check:web
npm run test:ui
cd firmware
pio run -e esp32s3
```

Pada Linux/macOS: `g++ -std=c++11 -Wall -Wextra -Werror -Ifirmware/include firmware/test/test_energy.cpp firmware/src/oled_frames.cpp -o /tmp/fort-test && /tmp/fort-test`. Untuk Chromium gunakan `npx playwright install chromium`. Alternatif Edge adalah `node tools/preview-test.cjs playwright msedge`.

## Source web dan dokumentasi

Ubah `web/Application.jsx`, view di `web/views/`, `web/i18n.js`, atau `web/styles.css`, kemudian `npm run build:web`. Jangan mengedit header gzip hasil generasi. `npm run check:web` membandingkan hasil build di memori. Guard PlatformIO memeriksa hash tanpa Node. Batas payload 160 KiB gzip.

English menjadi bahasa utama README/panduan. Pertahankan README.id.md dan docs/id sebagai edisi Indonesia. Edit SVG melalui `tools/generate_docs.py`, lalu periksa render dan overflow. Screenshot harus dilabeli simulasi. Pin, terminal Fort 58/59, dan perbedaan 70/COM harus konsisten dengan firmware serta manual.

Tes logika diperlukan untuk perubahan parsing, kualitas interval, recovery dan debounce. Tes browser memakai byte gzip firmware dengan API simulasi. Tes tidak membuktikan performa fisik UART/I2C/radio. Catat hasil nyata dan batasnya pada [VALIDATION](docs/id/VALIDATION.md).

Cache build lokal dan dependency terpasang boleh dihapus setelah verifikasi. PlatformIO membuat ulang library/hasil build, sedangkan `npm ci` memasang ulang dependency web. Header/manifest browser dan aset dokumentasi yang tersimpan di Git harus tetap tersedia.

## Versi, paket dan lisensi

Samakan version.h, package.json, lockfile, badge dan changelog. Build target N16R8 setelah edit terakhir. Salin BIN hardware terbaru dan merge sesuai [FLASHING](docs/id/FLASHING.md). Jalankan `python tools/package.py`. Manifest dan ZIP harus memuat source web, notice lisensi, panduan/aset, tanpa `.pio`, dependency terpasang atau file sementara.

Kode orisinal tetap GPL-3.0-only. React Bits memakai MIT + Commons Clause, bukan plain MIT atau GPL. Simpan lisensi dan provenance upstream-nya. React/React DOM/GSAP serta manual Fort tetap memakai ketentuan masing-masing. Lihat [NOTICE](NOTICE). Publikasi source dan publikasi binary merupakan operasi terpisah.

## Library meter bersama dan gaya penulisan

Perubahan transport serta diagnostik dilakukan pada FortTransport.h dan FortDiagnostics.h dalam firmware/lib/FortMeter, lalu diuji dengan source ModbusMaster sebenarnya. Header/generator lama merupakan jalur kompatibilitas, sedangkan perubahan source upstream harus dicatat pada hash serta patch provenance. INO mandiri dan ZIP Arduino tetap memakai source yang sama dengan produksi.

Tuliskan alur dan alasan desain dalam paragraf yang tersambung, dengan langkah konkret atau tabel ketika pembaca perlu bertindak dan membandingkan. Kurangi kalimat terpotong, titik koma berulang serta em dash agar penjelasan tetap alami dan cukup rinci untuk engineer.
