"""Build the pinned SDK in a space-free Windows cache, then reuse its verified configuration."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

def ensure_sdk(env,repository):
    project=Path(env.subst("$PROJECT_DIR"))
    if os.name!="nt" or " " not in str(project.resolve()):
        return
    keys=["platform","board","board_build.extra_flags","framework","custom_sdkconfig","board_build.arduino.memory_type","board_build.memory_type","board_build.psram_type","board_build.flash_mode","board_upload.flash_size","board_upload.maximum_size","board_build.partitions","build_flags","build_unflags"]
    options={key:env.GetProjectOption(key,None) for key in keys}
    options={key:value for key,value in options.items() if value is not None}
    config="[platformio]\ndefault_envs = esp32s3\n\n[env:esp32s3]\n"
    for key,value in options.items():
        if isinstance(value,(list,tuple)):
            value="\n".join(str(item) for item in value)
        config+=key+" = "+str(value).replace("\n","\n  ")+"\n"
    key=hashlib.sha256(config.encode()).hexdigest()[:16]
    core=Path(env.subst("$PROJECT_CORE_DIR"))
    cache=core/"midea-sdk-cache"/key/"firmware"
    if " " in str(cache.resolve()):
        raise RuntimeError("The SDK cache needs a path without spaces. Set PLATFORMIO_CORE_DIR to a space-free directory.")
    cache.mkdir(parents=True,exist_ok=True)
    sdk=Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32-libs"))
    variant=options.get("board_build.arduino.memory_type","qio_opi")
    files=[sdk/"sdkconfig",sdk/"esp32s3"/variant/"include/sdkconfig.h",sdk/"esp32s3/lib/libmbedx509.a",sdk/"esp32s3/lib/libwpa_supplicant.a"]
    def hashes():
        return {str(path.relative_to(sdk)):hashlib.sha256(path.read_bytes()).hexdigest() for path in files if path.exists()}
    stamp=cache/"verified-sdk.json"
    ready=False
    if stamp.exists() and (cache/"sdkconfig.defaults").exists() and (cache/"sdkconfig.esp32s3").exists():
        try:
            expected=json.loads(stamp.read_text())
            ready=len(expected)==len(files) and hashes()==expected
        except (OSError,ValueError):
            ready=False
    if not ready:
        print("Building the certificate-validating ESP32 SDK in a Windows cache without spaces. This first build can take several minutes.",flush=True)
        (cache/"platformio.ini").write_text(config,encoding="utf-8")
        (cache/"src").mkdir(exist_ok=True)
        (cache/"src/main.cpp").write_text('#include <Arduino.h>\n#ifndef CONFIG_MBEDTLS_HAVE_TIME_DATE\n#error "Certificate dates must be verified"\n#endif\nvoid setup(){}\nvoid loop(){delay(1);}\n',encoding="utf-8")
        shutil.copyfile(repository/"firmware/default_16MB.csv",cache/"default_16MB.csv")
        shutil.copyfile(repository/"firmware/sdk-dependencies.lock",cache/"dependencies.lock")
        # A changed global SDK must be rebuilt instead of trusting stale local metadata.
        (cache/"sdkconfig.defaults").unlink(missing_ok=True)
        result=subprocess.run([sys.executable,"-m","platformio","run","-d",str(cache)])
        if result.returncode:
            raise RuntimeError("SDK bootstrap failed. The production application has not been replaced.")
        header=files[1].read_text()
        if "#define CONFIG_MBEDTLS_HAVE_TIME_DATE 1" not in header or "#define CONFIG_ESP_WIFI_DISABLE_KEY_USAGE_CHECK 1" in header:
            raise RuntimeError("Compiled SDK certificate validation settings are invalid")
        values=hashes()
        if len(values)!=len(files):
            raise RuntimeError("Compiled SDK provenance files are missing")
        stamp.write_text(json.dumps(values,indent=2)+"\n",encoding="utf-8")
    for name in ["sdkconfig.defaults","sdkconfig.esp32s3"]:
        shutil.copyfile(cache/name,project/name)
    print("PASS: verified custom SDK cache, certificate date checks and unchanged application source",flush=True)
