"""Package already-built, verified local project. No flashing or network access."""
from pathlib import Path
import hashlib
import json
import zipfile
import re
import shutil
import subprocess
import sys
from datetime import datetime,timezone,timedelta

root = Path(__file__).resolve().parent.parent
subprocess.run([sys.executable,str(root/'tools/check_docs.py')],check=True)
version=re.search(r'FIRMWARE_VERSION\[\] = "([^"]+)"',(root/'firmware/include/version.h').read_text()).group(1)
app = root / "output/firmware/midea-fort-app.bin"
built = root / "firmware/.pio/build/esp32s3/firmware.bin"
if app.read_bytes() != built.read_bytes():
    raise SystemExit("Output app differs from current build. Copy and merge again.")
sources = [*sorted((root / "firmware/src").rglob("*.cpp")),
           *sorted((root / "firmware/include").rglob("*.h")),
           root / "firmware/platformio.ini"]
if any(p.stat().st_mtime > built.stat().st_mtime for p in sources):
    raise SystemExit("Sources changed after build; compile again before packaging.")
shutil.copy2(root/'docs/FLASHING.md',root/'output/firmware/FLASHING.md')
factory=(root/'output/firmware/midea-fort-factory.bin').read_bytes()
if factory[0x10000:]!=app.read_bytes():
    raise SystemExit('Factory image contains a different app. Merge again.')
for name,offset in [('bootloader.bin',0),('partitions.bin',0x8000),('boot_app0.bin',0xe000)]:
    part=(root/'output/firmware'/name).read_bytes()
    if factory[offset:offset+len(part)]!=part:raise SystemExit('Factory component mismatch: '+name)
root_names=['README.md','README.id.md','AGENTS.md','CHANGELOG.md','CHANGELOG.id.md','CONTRIBUTING.md','CONTRIBUTING.id.md','LICENSE','NOTICE','.gitignore',
            '.gitattributes','.editorconfig','.node-version','requirements-dev.txt','package.json','package-lock.json']
files=[*(root/name for name in root_names),*sources,
       *(p for p in sorted((root/'firmware/test').rglob('*')) if p.is_file() and p.suffix in ['.cpp','.h']),
       *(p for p in sorted((root/'examples').rglob('*')) if p.is_file() and '.pio' not in p.parts and p.suffix in ['.ino','.md','.ini','.csv']),
       *(p for p in sorted((root/'web').rglob('*')) if p.is_file()),
       *(p for p in sorted((root/'docs').rglob('*')) if p.is_file()),
       *(p for p in sorted((root/'.github').rglob('*')) if p.is_file()),
       *(p for p in sorted((root/'tools').glob('*')) if p.suffix in ['.py','.cjs','.ps1']),
       *(root/'output/firmware'/name for name in ['bootloader.bin','partitions.bin','boot_app0.bin','midea-fort-app.bin','midea-fort-factory.bin']),
       root/'output/firmware/FLASHING.md',root/'Fort Power Meter_User Manual_compressed.pdf']
manifest = {
    "version": version, "build_date_local": datetime.now(timezone(timedelta(hours=7))).date().isoformat(), "timezone": "Asia/Jakarta",
    "target": "ESP32-S3 N16R8", "build": "PlatformIO SUCCESS, espressif32 6.10.0",
    "license": "GPL-3.0-only",
    "license_scope": "Original project code; third-party terms are recorded in NOTICE",
    "web": json.loads((root / "web/bundle.json").read_text(encoding="utf-8"))["compressed_total"],
    "hardware_tested": False,
    "sha256": {str(p.relative_to(root)).replace("\\", "/"): hashlib.sha256(p.read_bytes()).hexdigest() for p in files},
}
manifest_path = root / "output/manifest.json"
manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
files.append(manifest_path)
archive = root / f"output/midea-fort-esp32s3-v{version}.zip"
with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
    for p in files:
        z.write(p, "midea-fort-esp32s3/" + p.relative_to(root).as_posix())
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    for rel,digest in manifest['sha256'].items():
        assert hashlib.sha256(z.read('midea-fort-esp32s3/'+rel)).hexdigest()==digest,rel
print(f"Packaged {len(files)} files; ZIP integrity OK; {archive.stat().st_size:,} bytes")
