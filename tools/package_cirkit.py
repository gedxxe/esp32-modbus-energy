"""Bundle the dedicated 8 MB Cirkit build using its three importer roles."""
from pathlib import Path
import struct
import zipfile

root = Path(__file__).resolve().parent.parent
build = root / 'firmware/.pio/build/cirkit'
files = {name: (build / name).read_bytes() for name in ['firmware.bin', 'bootloader.bin', 'partitions.bin']}
for name in ['firmware.bin', 'bootloader.bin']:
    image = files[name]
    assert image[0] == 0xe9 and struct.unpack_from('<H', image, 12)[0] == 9, f'{name}: not ESP32-S3'
    assert image[3] >> 4 == 3, f'{name}: header must declare 8 MB flash'
for offset in range(0, len(files['partitions.bin']), 32):
    row = files['partitions.bin'][offset:offset + 32]
    if row[:2] != b'\xaa\x50':
        break
    start, size = struct.unpack_from('<II', row, 4)
    assert start + size <= 8 * 1024 * 1024, 'Partition exceeds simulator flash size'
out = root / 'output/firmware/midea-fort-cirkit-8mb.zip'
out.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    for name, data in files.items():
        z.writestr(name, data)
with zipfile.ZipFile(out) as z:
    assert z.testzip() is None
    assert all(z.read(name) == data for name, data in files.items())
print(f'Cirkit ZIP ready: {out.name}; ESP32-S3 / 8 MB; {out.stat().st_size:,} bytes')
