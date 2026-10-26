"""PlatformIO pre-build guard. A committed valid web bundle needs no Node install."""
Import("env")
from pathlib import Path
import subprocess
import sys

root = Path(env.subst("$PROJECT_DIR")).parent
subprocess.run([sys.executable, str(root / "tools/check_web.py")], check=True)
