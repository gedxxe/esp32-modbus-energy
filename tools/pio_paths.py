"""Keep the custom SDK's CMake/Ninja build directory usable on Windows."""
Import("env")
import os
import shutil
import sys
import platformio
import runpy
from pathlib import Path
if platformio.__version__ != "6.1.19" or platformio.__title__ != "pioarduino":
    print(f"INCOMPATIBLE PLATFORMIO CORE: installed {platformio.__version__} ({platformio.__title__}); expected pioarduino 6.1.19.")
    print("No firmware compilation or upload was attempted. Do NOT upgrade the Arduino/ESP-IDF framework to fix this.")
    print("From the repository root, run: python tools/setup-core.py")
    print("Then restart VS Code/PlatformIO and run: pio run -d firmware -e esp32s3 -t upload")
    print(f"Core Python currently used: {sys.executable}")
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
# Ensure a newly-added private header changes SCons compiler flags even if
# __has_include() previously produced no dependency for the absent file.
# No URL, token or device ID is passed through build flags or printed.
validator = runpy.run_path(str(repository / "tools" / "check_iot_provision.py"))
try:
    header_present = validator["validate_header"](project / "include" / "iot_credentials.local.h")
except ValueError as error:
    print("IoT private provisioning build check: " + str(error))
    env.Exit(1)
env.Append(CPPDEFINES=[("IOT_EXPECT_LOCAL_PROVISION", int(header_present))])
lock = repository / "firmware" / "sdk-dependencies.lock"
project_lock = Path(env.subst("$PROJECT_DIR")) / "dependencies.lock"
if lock.exists() and (not project_lock.exists() or project_lock.read_bytes() != lock.read_bytes()):
    shutil.copyfile(lock, project_lock)
module=runpy.run_path(str(repository/"tools/sdk_bootstrap.py"))
module["ensure_sdk"](env,repository)
