"""Keep the custom SDK's CMake/Ninja build directory usable on Windows."""
Import("env")
import os
import shutil
import sys
import platformio
from pathlib import Path
if platformio.__version__ != "6.1.19" or platformio.__title__ != "pioarduino":
    print(f"This release requires PlatformIO Core 6.1.19, but {platformio.__version__} is running. Run python tools/setup-core.py from the repository root, then restart the extension or terminal.")
    print(f'Python interpreter: {sys.executable}')
    env.Exit(1)
managed = Path(env.subst("$PROJECT_CORE_DIR"))/"penv"/("Scripts/python.exe" if os.name=="nt" else "bin/python")
if os.path.normcase(os.path.abspath(sys.executable)) != os.path.normcase(os.path.abspath(managed)):
    print("Use the managed Core installed by python tools/setup-core.py. SDK child builds must use the same pinned interpreter.")
    print(f"Managed interpreter: {managed}")
    env.Exit(1)
project = Path(env.subst("$PROJECT_DIR"))
repository = next((parent for parent in [project,*project.parents] if (parent/"firmware/include/version.h").exists() and (parent/"tools/pio_paths.py").exists()),None)
if repository is None:
    print("Cannot locate the repository SDK dependency lock")
    env.Exit(1)
lock = repository / "firmware" / "sdk-dependencies.lock"
project_lock = Path(env.subst("$PROJECT_DIR")) / "dependencies.lock"
if lock.exists() and (not project_lock.exists() or project_lock.read_bytes() != lock.read_bytes()):
    shutil.copyfile(lock, project_lock)
import runpy
module=runpy.run_path(str(repository/"tools/sdk_bootstrap.py"))
module["ensure_sdk"](env,repository)
