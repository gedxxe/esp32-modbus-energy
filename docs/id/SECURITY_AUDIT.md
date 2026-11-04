# Audit keamanan repository dan pembuktian logika ADL400

**Tanggal:** 5 November 2026 · **Cakupan:** file yang terlacak Git, source code, dokumentasi, aset web, sketch referensi, pengujian dan pencarian byte teks pada aset biner. Ini bukan pengganti uji hardware atau audit forensik.

## Temuan dan perbaikan

Password Access Point (AP) pemulihan sebelumnya ditulis langsung di `firmware/include/board.h` dan sembilan file source/dokumentasi. Anggap password lama **telah terekspos**; jangan digunakan ulang. Firmware baru membuat 128 bit acak di perangkat, mengubahnya menjadi kata sandi WPA2 sepanjang 32 karakter, lalu menyimpannya di namespace NVS `apauth`. Jika NVS gagal, perangkat menggunakan password acak sementara untuk boot tersebut, bukan password bersama. Password dapat dilihat melalui **OLED → NETWORK / WIFI → AP password**; firmware tidak mencetaknya ke log atau mengembalikannya melalui API status.

**Upgrade akan mengganti password AP lama.** Sambungkan ulang memakai kata sandi yang ditampilkan di OLED. Menghapus password dari HEAD Git **tidak menghilangkannya dari commit lama, fork, clone atau cache**. Perubahan ini tidak melakukan rewrite history. Jika pernah digunakan ulang, seluruh kredensial terkait harus dirotasi. Perangkat dengan firmware lama tetap berisiko sampai di-flash ulang.

## Pemeriksaan otomatis

Jalankan `python tools/security_audit.py`. Pemeriksaan ini membaca seluruh file yang terlacak Git, mencari format token provider tertentu, private key, kredensial tertanam dan kata sandi yang tertulis pada dokumentasi. `.gitignore` diperketat agar file lokal, `.env`, sertifikat/key, build, log dan direktori sementara tidak mudah ikut ter-commit. CI menjalankan pemeriksaan ini dan unit test kredensial AP (pembuatan acak, bertahan sesudah reboot, rotasi data rusak, fallback saat penulisan NVS gagal).

```sh
python tools/security_audit.py --self-test
python tools/security_audit.py
```

**Batasan:** scanner berbasis pola tidak dapat memastikan 100% tidak ada rahasia. Screenshot, PDF terkompresi, metadata, riwayat Git, log CI lama, file yang tidak terlacak, dan NVS pada ESP32 fisik perlu diaudit terpisah. Nilai rahasia yang terdeteksi tidak dicetak oleh scanner.

## Risiko runtime yang belum diselesaikan

Dashboard berjalan lewat HTTP lokal, tanpa TLS dan tanpa akun pengguna tersendiri. Token perubahan yang dibuat setiap boot tidak dapat menggantikan kontrol akses jaringan. Jangan membuka endpoint HTTP ini ke internet; isolasi jaringan sebagaimana diperlukan. Password Wi-Fi client/Enterprise disimpan sebagai konfigurasi runtime perangkat, bukan konstanta dalam kode. Akses fisik terhadap flash dan keamanan lalu lintas HTTP merupakan ruang lingkup keamanan tersendiri.

## Audit pembacaan ADL400

Sketch yang pernah terbukti membaca meter fisik terdapat pada `docs/references/adl400-energy-field-reference.ino.txt`. Setting pengujiannya: **ID 220, 9600, 8N1, FC03, TX17/RX18/DIR16**. Jalur monitoring biasa memakai profil `acrel-adl400` dengan register:

| Data | Register / jumlah word | Format |
| --- | --- | --- |
| Total active energy | `0x0000 / 2` | UInt32 ABCD, /100 |
| Forward active energy (pembanding) | `0x000A / 2` | UInt32 ABCD |
| Reverse active energy (pembanding) | `0x0014 / 2` | UInt32 ABCD |
| Total reactive energy (pembanding) | `0x001E / 2` | UInt32 ABCD |
| PT | `0x008D / 1` | Baca terpisah |
| CT | `0x008E / 1` | Baca terpisah |

Hasil log **raw 9798**, PT 1, CT 80 menghasilkan **9798 / 100 × 1 × 80 = 7838,40 kWh**. Pembacaan LCD sekitar 7838,9 kWh, selisih 0,5 kWh, lebih kecil dari satu langkah raw sebesar 0,8 kWh setelah CT 80. Ini konsisten dengan resolusi counter/perbedaan waktu, tetapi bukan bukti bahwa kalibrasi dan halaman LCD sudah identik.

Pada firmware produksi, `FortTransport::readRawFC03` memeriksa CRC/frame/echo, `MeterReadings` menerima data, `profiles::primaryEnergy` menghitung skala, kemudian task acquisition mencatat checkpoint. PT dan CT diminta terpisah dan wajib tersedia sebelum perhitungan energi. Parameter float tambahan tidak menjadi syarat sampel kWh. Transaksi bersifat read-only. Host test mereplay temuan lapangan dan frame rusak, sedangkan CI mem-build firmware dan menguji UI. **Ini validasi logika dan integrasi otomatis, bukan uji end-to-end dengan perangkat ADL400 fisik.**

## Batas refaktor

Pembacaan meter, transport Modbus, registry, UI web, arsip energi dan diagnostik masih menggunakan modul masing-masing. Audit ini memisahkan pengelolaan password AP dan menambahkan pemeriksaan kredensial tanpa mengubah protokol Fort/Habuer, alamat register ADL400 atau format arsip checkpoint. Aset OLED/web yang besar tetap ada karena diperlukan untuk build yang dapat direproduksi.
