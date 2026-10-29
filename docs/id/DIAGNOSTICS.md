# Diagnostik pada firmware utama

[English](../DIAGNOSTICS.md) · **Bahasa Indonesia** · [Ringkasan](../../README.id.md)

Controller discovery bersama, registry flash bernama, ID sumber checkpoint serta codec setting yang dipisahkan dijelaskan pada [Pencarian meter](METER_DISCOVERY.md). Scanner tetap hanya memakai FC03 dan tidak menebak produsen dari nilai mentah.

Contoh khusus Fort di bawah tetap berlaku untuk Fort. Untuk Acrel atau Habuer, simpan model terlebih dahulu lalu ikuti [Profil meter](METER_PROFILES.md) serta [referensi Habuer](HABUER.md) untuk peta, konversi dan alur diagnostiknya.

Firmware 3.1.0 memakai source FortMeter/ModbusMaster yang sama untuk pembacaan normal, probe scanner dan sketch mandiri. Konsol menguji jalur pembacaan yang benar-benar dipakai aplikasi, dengan default manual pada TX17/RX18/DIR16, timeout 2000 ms, settling driver/receiver 100 µs dan jarak request 200 ms.


Konsol web juga menampilkan setting sesi serta counter bukti, dengan kontrol register dan setting uji terstruktur. Preset float primer menyiapkan perintah `primary` yang sama dengan sketch mandiri. [Panduan web terpadu](WEB.md) memuat peta fitur ke API beserta langkah koneksi AP/client.


## Langkah awal di panel

1. Hubungkan HP ke **Midea Engineering AP**, password **betahdijre12345**, lalu buka **http://192.168.4.1** dan pastikan footer Firmware 3.1.0. Samakan ID/baud/parity dengan menu CONi Fort dan pilih Manual direction untuk rangkaian DE-/RE terkonfirmasi.
2. Buka **Diagnostic console**, ketik `status` pada **Command input web**, lalu pilih **Run command**. Periksa ID, format, direction serta timing yang tercetak sebelum memulai tes. Baris ini merupakan input perangkat, bukan pesan ke chat dukungan.
3. Ketik `intermediate` sebagai perintah baru dan pilih Run command untuk mengirim contoh arus fasa A pada `0x0058`, dua register. Pada ID 1, TX harus `01 03 00 58 00 02 45 D8`. Output memuat RX mentah dan status ModbusMaster.
4. Masukkan `diagnose` secara terpisah jika memerlukan catatan lebih luas. Tes mengamati RX pasif tiga detik, mencoba probe dan melanjutkan plan register mentah, termasuk energi/rasio ketika decode aktif. Alamat pertama yang diam tidak menghentikan seluruh plan.
5. Tunggu `[REPORT_END]` pada **Device diagnostic output**, lalu pilih **Download output**. Simpan TXT tersebut dan salin OUTPUT atau filenya ke chat jika meminta analisis. Jangan masukkan label log kembali ke Command input.
6. Pilih **Resume measurement** setelah selesai dan pastikan peringatan maintenance hilang sebelum meninggalkan panel.

Tes UART aktif menghentikan sementara sampling RTC/interval serta memutus kontinuitas interval berjalan, sementara interval lengkap tetap dipertahankan. Setting dan nilai uji berada di RAM. Menutup panel atau browser tidak melanjutkan pencatatan, sehingga buka kembali dashboard untuk memeriksa sesi yang masih aktif. `status`, `report`, `help`, `compare` dan `plan show` dapat membaca informasi tanpa memasuki maintenance.

## Arti hasil transaksi

| Output | Makna dan tindak lanjut |
| --- | --- |
| `0x00 OK` dan `[VALID_RESPONSE]` | ID, FC03, count, panjang serta CRC sah, bandingkan nilai dengan halaman Fort yang sesuai. |
| `0x01–0x04` dan `[COMM_VALID_EXCEPTION]` | Exception cocok dan CRC terverifikasi, tetapi data yang diminta tidak dibaca, periksa alamat/count serta dukungan register. |
| `0xE0 WRONG_SLAVE_ID` | Prefix memakai ID lain atau rusak, periksa HEX tanpa menganggapnya balasan Fort. |
| `0xE1 WRONG_FUNCTION` | Fungsi yang diterima berbeda dari FC03/exception terkait. |
| `0xE2 RESPONSE_TIMEOUT` | Respons lengkap tidak tiba dalam deadline, termasuk kemungkinan RX parsial. |
| `0xE3 CRC_ERROR` | Integritas respons gagal, termasuk CRC exception. |
| `0xE4 ABORTED` | Stop/Resume memutus penantian tanpa menerima nilai baru. |
| `0xE5 LOCAL_UART_ERROR` | Driver lokal atau penulisan byte gagal sehingga jalur eksternal belum dapat dinilai. |
| `0xE6 INVALID_FRAME_LENGTH_OR_ECHO` | Bentuk respons tidak sesuai atau request dipantulkan persis, tidak ada byte yang dihapus agar tampak sah. |
| `0xE7 RX_OVERFLOW` | Input melewati batas penerimaan/capture dan ditolak. |

RX, CRC gagal dan BREAK merupakan observasi jalur UART lokal, bukan bukti mandiri bahwa Fort membalas atau modul sehat. Ketika A/B dilepas, aktivitas yang diterima berasal dari rangkaian yang masih terhubung. Sebaliknya, slave boleh diam sebelum diminta, sehingga tes pasif yang sunyi tidak membuktikan komponen rusak. Log mempertahankan ketidakpastian tersebut agar kesimpulan tidak melampaui bukti.

## Pilih tes terbatas

Masukkan **satu baris setiap kali di Command input web dan klik Run command untuk baris tersebut**. Alamat memakai PDU heksadesimal dan jumlah memakai register 16-bit, tanpa konversi offset 40001.

| Tujuan | Perintah |
| --- | --- |
| Contoh arus manual | `probe 0058 2`, lalu `intermediate` |
| Tegangan, daya atau frekuensi langsung | `reg 0042 2`, `reg 006A 2`, `reg 0080 1` |
| Counter sekunder | `reg 0082 2` |
| Dua kandidat urutan float primer | `primary` |
| RX tanpa TX | `listen 5` |
| Observasi TTL dengan pull lemah | `rxcheck` |
| Bukti tersimpan tanpa request baru | `report` |
| Hentikan tes serta antrean lama | `x` atau Stop test |
| Pulihkan koneksi dan pencatatan | `resume` atau Resume measurement |

Parameter runtime mencakup `id 1`, `baud 9600`, `parity N`, `pins 18 17`, `direction manual`, `settle 100`, `gap 200`, `timeout 2000` serta `interval 2000`. `direction auto` hanya untuk rangkaian yang memang mengendalikan arah otomatis, bukan pengganti kabel DE-/RE pada transceiver manual. Pin arah tidak dapat dipakai sebagai pin UART selama mode manual aktif.

Plan raw menggunakan `plan clear`, maksimal delapan `plan add ADDRESS WORDS`, dan `plan show`. Profil energi uji dapat diubah melalui `energy 0082`, `ratios 002C`, `scale 0.001`, `order normal/swap` dan `decode on/off`, tetapi seluruh perubahan itu hanya berlaku di sesi RAM. Perbedaan panjang pada tabel manual dapat diaudit dengan satu/dua/tiga word. Float primer tidak otomatis dianggap integer sekunder hanya karena salah satu hasil terlihat masuk akal.

## Cari setting koneksi

Scanner produksi memakai baud/parity/direction tersimpan serta rentang ID terpilih, dengan `0x0058/2` sebagai probe awal. Respons data atau exception sah diulang sebelum ditawarkan. Apply result menyimpan setting pembaca, sedangkan energi/rasio tetap diperiksa oleh akuisisi normal. Discovery tidak mengautentikasi model meter atau memverifikasi skala fisik.

Dengan timeout dua detik, 16 ID yang diam membutuhkan sekitar 35 detik termasuk jeda request. Matriks 1–247 dan 18 format dapat mencapai sekitar tiga jam, sehingga mulai dari setting CONi yang diketahui atau rentang kecil. Daftar konsol dapat dipersempit lewat `range 1 16`, `scanmode current`, `bauds 9600 19200`, `parities NE` dan `scan`. Perintah `x` diproses selama penantian. Scanner produksi dan tes aktif konsol tidak memiliki UART bersamaan.

## Aliran komunikasi dan monitor proses

Panel komunikasi mengikuti event TX diantrekan/selesai, RX pertama serta hasil transaksi. Ring 32 event memperlihatkan gap urutan, sementara browser mereplay selang timestamp 1× dengan delay polling/jaringan tanpa gerakan dekoratif ketika diam. Tampilan bukan pengukuran propagasi A/B atau bukti bahwa perangkat jauh menerima request.

Monitor proses pada Perangkat & arsip menampilkan sampel non-idle core, heap/PSRAM, state/priority task dan stack high-water. Panel rinci hanya polling ketika terbuka. Nilai stack merupakan minimum historis dan persentase tick bukan profiler instruksi. Gunakan untuk memeriksa headroom sebelum menguji beban serta ketahanan pada perangkat fisik.

## Alternatif Serial Monitor pada firmware utama

Buka **Serial Monitor Arduino IDE/PlatformIO pada 115200 baud, Newline**. Ketik `debug` di **INPUT Serial Monitor**, tekan Enter/Send, lalu masukkan `status` dan `intermediate` sebagai baris terpisah. `x` menghentikan tes dan `resume` memulihkan pengukuran. OUTPUT dapat disalin ke chat dukungan, sedangkan log web tetap bisa diunduh tanpa USB.

## Perbandingan fisik berpasangan

`check connected`, `check isolated` dan `compare` menyimpan capture berlabel operator dengan pemeriksaan konfigurasi serta completion. Gunakan label tersebut hanya ketika A/B benar-benar diubah secara fisik di modul antara dua capture. Menjalankan kedua perintah pada koneksi yang sama bukan deteksi kabel otomatis. [Delapan resep lapangan](../../examples/FortSerialDebug/README.id.md#pilih-pengujian-berdasarkan-hasil) menyediakan variasi terbatas untuk RX pasif, format, map mentah dan urutan word.


[Diagnostik referensi Habuer](HABUER.md) memakai konsol dan transport yang sama. Pilih profil sebelum membaca atau melakukan scan.
