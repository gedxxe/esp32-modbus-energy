"""Install the pinned Core in the managed environment used by SDK child builds."""
from pathlib import Path
import os
import subprocess
import sys
import venv

root=Path(__file__).resolve().parent.parent
core=Path(os.environ.get("PLATFORMIO_CORE_DIR",str(Path.home()/".platformio")))
python=core/"penv"/("Scripts/python.exe" if os.name=="nt" else "bin/python")
if not python.exists():
    venv.EnvBuilder(with_pip=True).create(core/"penv")
pin=next(line.strip() for line in (root/"requirements-dev.txt").read_text().splitlines() if line.startswith("pioarduino=="))
# Both distributions install the platformio module. Keep only the pinned provider.
old=subprocess.run([str(python),"-m","pip","show","platformio"],capture_output=True)
if old.returncode==0:
    subprocess.run([str(python),"-m","pip","uninstall","-y","platformio"],check=True)
subprocess.run([str(python),"-m","pip","install","--force-reinstall","--no-deps",pin],check=True)
subprocess.run([str(python),"-m","pip","install","-r",str(root/"requirements-dev.txt")],check=True)
subprocess.run([str(python),"-c","import platformio; assert platformio.__version__ == '6.1.19' and platformio.__title__ == 'pioarduino'"],check=True)
print("Pinned Core ready. Restart the PlatformIO terminal or use:")
print(str(python)+" -m platformio run -d firmware")
