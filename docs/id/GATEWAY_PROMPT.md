# Prompt pengembangan gateway ESP32-C6 terpisah

[English](../GATEWAY_PROMPT.md) · **Bahasa Indonesia** · [Setup PTMP](LORA_PTMP.md) · [Kontrak radio](LORA_PROTOCOL.md)

Salin prompt di bawah ke **INPUT chat coding-agent baru yang dibuka pada repository gateway terpisah**. Isinya permintaan pengembangan, bukan perintah Serial Monitor maupun Command input pada web S3. Beri chat tersebut akses baca ke repository station ini atau ZIP source-nya. Repository node tetap menjadi referensi dan tidak ditimpa menjadi build receiver.

Kontrak ini mengikuti station 3.5.0 dengan wire version Energy Station v1, tetap cocok dengan 3.4.0. Periksa source sebenarnya dan test vector ketika mengerjakan gateway. Teks firmware `3.4.0` pada fixture HELLO publik lama sengaja dipertahankan dan tidak mengubah versi protokol.

## Prompt untuk disalin

```text
Buat firmware gateway PTMP ESP32-C6 + SX1262 raw-LoRa pada repository terpisah ini sampai siap compile. Project node Midea Energy Station ESP32-S3 menjadi referensi interoperabilitas read-only, bukan target build gateway.

Mulai dengan membaca source station berikut:
- docs/LORA_PROTOCOL.md dan docs/LORA_PTMP.md
- firmware/include/lora_protocol.h
- firmware/include/checkpoint_log.h, energy.h dan clock_guard.h
- firmware/lib/FortMeter/src/MeterProfiles.h
- firmware/src/lora_service.cpp
- docs/lora-v1-vector.json dan tools/check_lora.py
- docs/LORA.md dan docs/ENTERPRISE_WIFI.md

Periksa board C6, modul radio, GPIO, SDK dan tooling sebenarnya sebelum menentukan wiring. Pin framework yang kompatibel serta RadioLib 7.8.1. Aplikasi C6 memakai satu core performa tinggi. Jangan menyalin affinity core 1, asumsi PSRAM, GPIO, polling Modbus, OLED atau RGB/RMT S3. Buat tabel pin serta SVG setelah memeriksa board/modul. TCXO dan RF switch mengikuti rangkaian modul gateway, bukan asumsi bahwa seluruh SX1262 sama dengan kristal dan switch DIO2 Ra-01SH.

Topologi terdiri dari beberapa station S3 menuju satu gateway C6 di kantor, kemudian Wi-Fi menuju backend yang dapat dikonfigurasi. Gunakan raw LoRa, bukan LoRaWAN, mesh, receiver broadcast saja, teks atau JSON melalui RF. Default carrier 915 MHz dapat diedit. Default PHY ialah 125 kHz, SF7, CR4/5, preamble 8, sync logis 0x12, explicit header, IQ normal dan CRC LoRa. Ikuti semantik sync byte RadioLib pada node. Radio nonaktif sampai dikonfigurasi. Dokumentasikan batas wilayah instalasi tanpa mengganti default pemilik secara diam-diam.

Buat inventori gateway terbatas, awalnya 16 entri dengan konstanta kapasitas yang jelas. Setiap entri berisi ID jaringan, ID node unik, nama, key per-node 32 byte bukan nol, status enabled dan catatan commissioning. Default ID gateway 1, ID jaringan 1, contoh node 2, 3 dan 4. Alamat node dan gateway wajib berbeda. Slave ID Modbus, profile ID serta saved-meter ID lokal hanyalah metadata dan tidak menjadi source ID radio. Jangan tampilkan key dalam log, status JSON atau browser storage. Konfigurasi key memakai dashboard lokal yang terotorisasi, seperti station melalui antarmuka AP atau client. Key tetap tidak boleh masuk status maupun log.

Implementasikan codec biner Energy Station v1 sesuai source, dengan little endian eksplisit tanpa cast struct native. Header 24 byte, payload maksimal 112, tag 16 dan total maksimal 152. Verifikasi 16 byte pertama HMAC-SHA256 atas header dan payload secara constant time sebelum menerima frame. Periksa magic, versi, reserved, panjang, jaringan, sumber, tujuan, session dan tipe. Lookup key harus terbatas serta menolak node tak dikenal/nonaktif. Protokol saat ini mengautentikasi plaintext. Jangan diam-diam menambah enkripsi atau layout baru yang tidak dikenali node.

Dukung HELLO=1, ACK=2, TELEMETRY=3, POLL=4 dan CHECKPOINT=5. ACK membalik alamat, mempertahankan jaringan dan session node, menyalin sequence tepat dari paket asal serta membawa satu byte tipe yang diakui. Jangan membuat session gateway atau menambah sequence pada ACK tersebut. Duplikat valid mendapat ACK lagi tanpa penyimpanan ganda. ACK node/session/tipe lain tidak boleh menyelesaikan transaksi yang berbeda.

Pertahankan candidate/current session per node. HELLO bertanda tangan benar menjadi kandidat dan belum membuktikan keterjangkauan terkini setelah replay atau reboot receiver. Balas ACK, lalu lakukan POLL live baru dan periksa respons terautentikasi yang berkorelasi sebelum mengganti session aktif. Data lama tetap tersedia selama kandidat belum terbukti. Batasi laju kandidat serta tolak promosi data/session kedaluwarsa. Sequence POLL gateway harus meningkat dalam session node yang sama dan bertahan saat restart, menggunakan reservasi blok ber-CRC yang ditulis sebelum TX. Jangan mulai ulang dari 1 jika node masih mengingat sequence lebih besar. Tangani kehilangan state sequence secara eksplisit, tanpa menebak angka.

Radio dimiliki worker nonblocking dengan antrean terbatas. ISR hanya mengubah flag IRQ. Kembali RX setelah TX_DONE. Batasi tunggu BUSY, reset/startup, antrean ACK, retry dan operasi jaringan. Satu SX1262 menerima satu PHY terpilih dan tidak dapat RX saat TX. Gunakan satu anggaran airtime gateway bagi seluruh ACK serta POLL. ACK diprioritaskan, ACK tertunda identik digabung, pekerjaan usang kedaluwarsa dan permintaan opsional dibagi round-robin. Utamakan telemetry periodik pasif dibanding POLL live berulang. Jalankan satu transfer arsip global sambil menerima uplink lain di antara fragmen. Polling tidak membungkam kiriman mandiri node dan bukan TDMA bebas tabrakan. Hitung kapasitas dari airtime frame sebenarnya dengan margin. Default ACK node 10 detik, contention TX pertama bisa 30 detik, dan ACK terlambat hanya diterima saat frame identik masih aktif. Deadline POLL tidak sama dengan deadline ACK.

Dekode payload telemetry 32 byte persis, termasuk korelasi request, UTC perangkat, usia sampel, flag validitas, profile ID immutable, saved-meter ID lokal, hari WIB, revision dan kWh IEEE754 binary64. Nilai hilang/kedaluwarsa tetap unavailable, bukan nol. Simpan waktu terima gateway terpisah dari waktu node. Jangan mengalikan CT/PT lagi dan jangan menjadikan nilai live sebagai checkpoint terjadwal.

POLL mode 0 meminta live dan mode 1 meminta checkpoint. Hari nol memilih hari capture node saat ini. Request lima byte berisi mode serta hari little endian. Pakai session node saat ini dan sequence POLL baru yang meningkat. Cocokkan respons dengan jaringan, node, session dan request. Hari yang tidak tersedia saat ini tidak mengirim fragmen. Tampilkan timeout/unavailable, bukan record kosong yang berhasil.

Rakit CHECKPOINT memakai header fragmen 12 byte, chunk maksimal 100 byte dan ukuran akhir tepat 446. Periksa batas, overlap dan kesamaan byte duplikat sebelum menerima urutan ulang. Hentikan transfer parsial saat session berubah atau timeout terbatas. Node dapat menyisipkan telemetry live miliknya di antara fragmen. Proses telemetry tanpa mereset assembly arsip yang berkorelasi. Satu transfer adalah snapshot beku. Balas ACK pada fragmen yang diterima, kemudian jalankan CRC serta validasi semantik sesuai checkpoint_log.h sebelum menerbitkan record hari. Pertahankan sepuluh jadwal WIB, timestamp request/response, nilai recorded immutable dan ID sumber/rasio. Gunakan decoder pure yang disediakan beserta dependensinya bila sesuai, dengan lisensi GPL-3.0-only tetap ada. Jangan backfill checkpoint dari telemetry belakangan atau menghitung ulang map meter di gateway.

Pisahkan status penerimaan radio, pengumpulan durable lokal dan pengiriman backend. Buat outbox lokal ber-CRC dengan verified write, recovery restart dan retensi terbatas. Jangan memformat storage gagal otomatis atau menyebut antrean RAM durable. ACK bukan bukti commit database dan node tetap menyimpan record setelah ACK. Gateway yang crash dapat meminta hari tersebut kembali. Backend harus idempotent berdasarkan jaringan/node, hari/revision dan identitas capture/sumber. Retry tidak boleh menggandakan energi atau menimpa titik lengkap dengan versi lama. Definisikan schema payload serta endpoint HTTPS yang dapat dikonfigurasi tanpa mengarang URL atau kredensial server. Backend tidak tersedia tidak boleh menghentikan radio.

Sediakan AP/client/dual dan scan Wi-Fi tanpa SSID perusahaan hardcoded. Dukung Personal serta PEAP/EAP-MSCHAPv2 opsional melalui API resmi SDK terpin. Pertahankan validasi chain/tanggal sertifikat, public CA bundle default, private CA opsional dan nama DNS sertifikat opsional. Identity mengikuti username kecuali diedit sengaja. Jangan mematikan validasi atau mewajibkan unggah sertifikat tanpa alasan. Kredensial baru hanya melalui AP terlindungi. Setiap form menampilkan feedback submit/progress/error di dekat tombol. Sediakan pengaturan jam lokal yang disengaja sebelum Enterprise/TLS jika waktu tepercaya belum tersedia. Jangan menganggap C6 memiliki DS3231, NVS berisi waktu terkini atau SNTP dapat berjalan sebelum autentikasi Wi-Fi. Bootstrap waktu lewat radio harus memakai node berwenang yang terautentikasi serta terbukti fresh, dengan kebijakan eksplisit.

Buat dashboard English/Indonesia yang ringkas untuk inventori node, RF, session/pairing, waktu/usia data, nilai, pengambilan checkpoint, outbox/backend, RSSI/SNR, counter ACK/retry/auth/CRC dan log terbatas. Bedakan configured, inisialisasi chip, pertukaran radio terautentikasi, freshness data serta durability database. Representasi animasi harus jelas merupakan skema bila bukan pengukuran fisik.

Selesaikan verifikasi sebelum packaging: compile C6 sebenarnya, vector codec/HMAC publik, tiga node/key independen, penolakan node asing/replay, ACK terlambat/duplikat, reservasi sequence melewati reset, challenge candidate session, telemetry bersamaan satu transfer arsip, korupsi/urutan ulang fragmen, fault power-loss outbox, backend putus/idempotence serta tes web dari byte hasil build untuk kedua bahasa. Pisahkan mock software dari uji RF/daya yang masih pending. Serahkan source, dependency terpin, SVG/netlist, catatan kompatibilitas, langkah setup dan binary tervalidasi dengan manifest. Jangan mengklaim pairing lapangan, jangkauan atau kehilangan data nol tanpa bukti hardware.
```

Hasil pekerjaan gateway perlu menyebut target, framework, versi kontrak dan pemeriksaan yang benar-benar selesai. Uji hardware pertama memakai satu node sebelum menambah dua lainnya. [Panduan PTMP](LORA_PTMP.md) menjelaskan pengaturan ID dan langkah operator.
