"""PlatformIO pre-build guard. A committed valid web bundle needs no Node install."""
Import("env")
from pathlib import Path
import subprocess
import sys

root = Path(env.subst("$PROJECT_DIR")).parent
result = subprocess.run([sys.executable, str(root / "tools/check_web.py")])
if result.returncode:
    print("Web assets are out of date. From the repository root, run: npm ci && npm run build:web")
    print("Then build again from firmware/: pio run -e esp32s3")
    env.Exit(result.returncode)

subprocess.run([sys.executable, str(root / "tools/build-diagnostic.py"), "--check"], check=True)

subprocess.run([sys.executable, str(root / "tools/build-oled.py"), "--check"], check=True)
