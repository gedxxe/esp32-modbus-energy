# Protokol radio Energy Station v1

[English](../LORA_PROTOCOL.md) · **Bahasa Indonesia** · [Konfigurasi dan wiring](LORA.md)

Spesifikasi ini merupakan kontrak antara node S3 dan gateway ESP32-C6 yang dikembangkan terpisah. Transport memakai raw LoRa, explicit header, IQ normal, CRC fisik dan key bersama 32 byte. Ini bukan LoRaWAN. Gateway harus membaca format biner tersebut, bukan menganggap paket sebagai JSON atau teks.

## Representasi dan parameter default

Kedua sisi menyamakan carrier, bandwidth, SF, coding denominator, preamble dan sync byte. Default adalah 915 MHz, 125 kHz, SF7, CR4/5, preamble 8 dan sync 0x12. Pada RadioLib SX126x, sync logis 0x12 dipetakan ke register 0x1424. Integer aplikasi multibyte memakai little endian, terpisah dari urutan register Modbus. Jangan mengirim struct dengan padding native.

Paket berisi header 24 byte, payload 0–112 byte dan tag 16 byte, maksimum 152 byte. Tag adalah 16 byte pertama `HMAC-SHA256(key, header || payload)` dan diperiksa constant time sebelum frame ditafsirkan. CRC fisik LoRa berbeda dari CRC capture. Payload diautentikasi tetapi tidak dienkripsi.

| Offset | Byte | Arti |
| --- | --- | --- |
| 0 | 2 | ASCII `ES` |
| 2 | 1 | Versi protokol 1 |
| 3 | 1 | HELLO 1, ACK 2, TELEMETRY 3, POLL 4 atau CHECKPOINT 5 |
| 4 | 2 | Network ID bukan nol |
| 6 | 2 | ID sumber 1–65534 |
| 8 | 2 | ID tujuan 1–65534, berbeda dari sumber |
| 10 | 1 | Panjang payload tepat, maksimum 112 |
| 11 | 1 | Reserved, nol |
| 12 | 4 | Sequence bukan nol |
| 16 | 8 | Session nonce node bukan nol |
| 24 | variabel | Payload |
| 24 + panjang | 16 | Tag HMAC-SHA256 terpotong |

Versi, reserved bit, panjang, alamat, jaringan, session dan autentikasi yang salah ditolak. Node membuat nonce acak 64 bit saat boot, perubahan radio atau pairing ulang. Session bukan timestamp RTC. Sequence node naik dalam session yang sama dan session diganti sebelum wrap. Sequence POLL gateway meningkat dengan jarak maju signed 32 bit dan dimulai ulang pada session node baru.

## HELLO dan ACK

HELLO memiliki payload 24 byte. Byte 0 berisi schema checkpoint3, byte 1 capability bit 0–2 untuk live, retrieval checkpoint dan autentikasi, lalu byte 2–23 berisi versi firmware berakhiran nol dan padding nol. Gateway memeriksa HMAC serta ID sebelum menerima session.

ACK memiliki satu byte tipe yang diakui. ID sumber/tujuan ditukar, network/session dipertahankan dan sequence disalin dari paket yang diakui. ACK HELLO berisi 1, telemetry3 dan checkpoint5. ACK tidak cocok tidak menyelesaikan paket aktif. Status paired hanya muncul setelah HELLO node mendapat ACK terautentikasi yang benar.

Gateway beberapa node sebaiknya menyimpan key terpisah untuk setiap ID node. Gateway menghapus duplikasi berdasarkan `(network, node, session, sequence)` dan mengirim ulang ACK untuk duplikat valid tanpa menggandakan penyimpanan. Retry node mempertahankan byte dan sequence sama. Key tidak disiarkan atau dinegosiasikan melalui frame tanpa autentikasi.

## TELEMETRY

Payload 32 byte membawa engineering kWh yang sudah dikonversi menurut profil meter. Gateway tidak mengalikan CT/PT kembali.

| Offset payload | Byte | Arti |
| --- | --- | --- |
| 0 | 4 | Sequence POLL terkait, atau 0 untuk periodik |
| 4 | 4 | Referensi UTC perangkat, 0 jika tidak tersedia |
| 8 | 4 | Usia sampel meter dalam milidetik |
| 12 | 1 | Flag validitas |
| 13 | 1 | ID profil meter immutable |
| 14 | 2 | ID inventori tersimpan, 0 jika belum terikat |
| 16 | 4 | Nomor hari WIB saat ini |
| 20 | 4 | Revision checkpoint |
| 24 | 8 | IEEE754 binary64 engineering kWh little endian |

Flag bit 0 berarti energi valid tersedia dan tidak lebih tua dari 15 detik, bit 1 RTC valid, bit 2 record checkpoint durable lokal, bit 3 kontak meter online dan bit 4 diagnostik/discovery/scan aktif. Saat bit 0 nol, kWh berarti tidak tersedia walaupun placeholder nol. Jangan mengubah data hilang menjadi pengukuran nol. Usia mencakup waktu tunggu worker. UTC berasal dari referensi owner dan bukan browser.

## POLL dan fragmen checkpoint

POLL memiliki lima byte. Byte 0 memilih 0 untuk live atau 1 untuk checkpoint, lalu byte 1–4 berisi nomor hari WIB. Hari 0 memilih hari capture saat ini. Poll diterima setelah pairing, sequence replay ditolak dan hanya satu query arsip dapat menunggu. Poll sibuk/ditolak tidak menyatakan pembacaan sukses. Gunakan sequence baru meningkat ketika respons kedaluwarsa. Versi 1 tidak menyediakan tulis register, set RTC, reboot atau konfigurasi radio lewat udara.

Poll live menghasilkan telemetry dengan sequence request pada byte 0–3. Poll arsip menghasilkan CHECKPOINT. Setiap fragmen memiliki sequence node baru dan membutuhkan ACK sendiri.

| Offset payload fragmen | Byte | Arti |
| --- | --- | --- |
| 0 | 4 | Sequence POLL terkait |
| 4 | 4 | Nomor hari WIB sebenarnya |
| 8 | 2 | Offset byte pada record |
| 10 | 2 | Ukuran total record 446 |
| 12 | 1–100 | Byte record berurutan |

Fragmen terakhir lebih pendek. Periksa batas, overlap, isi duplikat, session, sequence request dan hari. Satu transfer memakai snapshot beku. Record schema 3 mengikuti `checkpoint_log.h`, termasuk magic, ukuran, validasi semantik dan CRC16 polinomial Modbus little endian. Timestamp capture dan ID sumber tetap immutable. Schema1/2 lama dibaca node lalu diserialisasi ke schema 3 tanpa membuat ID inventori yang tidak ada.

Validasi record lengkap sebelum memasukkannya ke database. Gunakan hari, revision dan identitas capture/sumber untuk penyimpanan idempotent. ACK radio berarti penerimaan paket dan belum membuktikan commit database. Log lokal dipertahankan terlepas dari ACK, sehingga seluruh hari dapat diminta lagi setelah gateway crash. Hari yang tidak tersedia tidak menghasilkan fragmen dan menambah drop counter. Tampilkan unavailable, bukan nol atau record kosong yang sukses.

## Waktu dan batas

Satu paket node menunggu penyelesaian. Setelah TX, radio kembali RX kontinu dan menunggu ACK dengan default 10 detik. Dua retry berarti tiga percobaan total. Backoff acak dan pacing 1% per boot membatasi seluruh frame keluar. Telemetry yang belum pernah TX diperbarui sebelum percobaan pertama. Paket live lebih tua dari 15 detik dibuang agar tidak diulang sebagai nilai segar, sedangkan fragmen record tetap merupakan data bertanggal. Pada parameter lambat, anggaran ini dapat menunda retry lebih lama daripada timeout ACK. Gateway perlu menjadwalkan respons half duplex dan laju poll beberapa node sesuai kondisi itu.

Setelah retry habis, sesi menjadi unpaired dan HELLO dicoba kemudian. ESP32 tidak direset dan log tidak dihapus. Telemetry mengambil sampel terbaru. Antrean arsip/kontrol terbatas. Core 1 membaca arsip di luar capture mendesak, sedangkan core 0 memiliki SPI/radio. Jangan menganggap node melayani arus request tanpa batas.

[Test vector publik](../lora-v1-vector.json) memuat body HELLO, key uji, tag dan paket lengkap. Key publik itu tidak boleh digunakan pada instalasi. Fixture membantu tes codec/HMAC gateway, bukan bukti RF. Firmware gateway, database durable, jangkauan dan pairing fisik tetap menjadi pekerjaan penerimaan terpisah.
