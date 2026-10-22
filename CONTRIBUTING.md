# Kontribusi dan pemeliharaan

Baca [AGENTS.md](AGENTS.md) untuk invariants firmware dan fakta hardware. [README](README.md) adalah panduan pemasangan utama.

## Lingkungan

- PlatformIO Core 6.1.16 (`python -m pip install -r requirements-dev.txt`).
- g++ dengan dukungan C++11 untuk regression test tanpa board.
- Python 3.10+ untuk generator SVG, pengecekan docs dan paket. CI memakai Python 3.13 untuk docs/logika, serta 3.11 untuk PlatformIO.
- Node 24 + `npm ci` hanya diperlukan untuk browser test.

Install toolchain/library ESP32 secara otomatis dengan `pio run` dari `firmware`. Versi source of truth ada di [platformio.ini](firmware/platformio.ini); jangan mengganti versi library hanya pada badge/README.

## Perubahan firmware

1. Pertahankan ownership UART/Preferences pada task akuisisi core 1 dan gunakan queue/mutex sesuai desain.
2. Tambahkan regression test untuk perubahan perhitungan, kualitas data, recovery atau debounce.
3. Jalankan `./tools/test.ps1` pada Windows. Pada Linux/macOS:

   ```sh
   mkdir -p tmp
   g++ -std=c++11 -Wall -Wextra -Werror -Ifirmware/include firmware/test/test_energy.cpp -o tmp/fort-test
   ./tmp/fort-test
   ```

4. Dari `firmware`, jalankan `pio run` dan catat hasil aktual pada [VALIDATION](docs/VALIDATION.md).
5. Perubahan jalur hardware tetap membutuhkan [commissioning](docs/COMMISSIONING.md); compile/simulasi tidak membuktikan hasil hardware.

## Perubahan visual dan dokumentasi

SVG dibuat oleh [generate_docs.py](tools/generate_docs.py), dengan versi dari firmware. Jalankan:

```sh
python tools/generate_docs.py
python tools/check_docs.py
```

Ubah generator jika mengubah diagram, lalu periksa PNG render di `output/previews/`. Pertahankan label simulated pada screenshot UI dan ilustrasi OLED. Semua gambar README harus ada di `docs/assets/`, dengan link relatif dan case tepat agar tampil di GitHub/Linux.

Browser test memakai HTML firmware asli dengan HTTP mock:

```sh
npm ci
npx playwright install chromium
npm run test:ui
```

Alternatif Windows dengan Edge: `node tools/preview-test.cjs playwright msedge`. Untuk runtime yang menyediakan Playwright di lokasi lain, gunakan path modul sebagai argumen pertama. Script juga merender dan mengecek batas SVG jika aset dokumentasi tersedia.

## Versi dan rilis

Versi firmware di [version.h](firmware/include/version.h). Saat membuat patch release, samakan versi development tools di `package.json` dan perbarui [CHANGELOG](CHANGELOG.md). `tools/check_docs.py` memeriksa konsistensinya.

Setelah build terakhir lulus, ikuti [FLASHING](docs/FLASHING.md) untuk menyiapkan binary. `python tools/package.py` memverifikasi kecocokan app dengan build terkini, menyertakan source/docs/aset, membuat manifest SHA-256 dan menguji integritas ZIP. `output/` tidak dimasukkan ke Git; bagikan hasilnya sebagai artefak rilis.

Workflow [CI](.github/workflows/ci.yml) menyediakan tiga job: logic/docs, firmware dan browser. Definisi workflow menggunakan dokumentasi resmi [checkout](https://github.com/actions/checkout), [setup-python](https://github.com/actions/setup-python), [setup-node](https://github.com/actions/setup-node) dan [upload-artifact](https://github.com/actions/upload-artifact). Workflow belum dijalankan pada repository GitHub dalam sesi lokal ini.

## Lisensi dan publikasi

Pemilik memilih **GNU GPL versi 3 saja**, dengan identifier `GPL-3.0-only`. Pertahankan [LICENSE](LICENSE), [NOTICE](NOTICE) dan metadata lisensi saat mendistribusikan project. Dependency dan manual Fort tidak dilisensikan ulang oleh project ini. Menyiapkan file untuk GitHub tidak berarti menginisialisasi remote, memublikasikan repository, atau melakukan push.
