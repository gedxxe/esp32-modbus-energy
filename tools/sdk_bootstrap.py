"""Build the pinned SDK once in a shared cache, then reuse its verified configuration."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

def ensure_sdk(env,repository):
    project=Path(env.subst("$PROJECT_DIR"))
    keys=["platform","board","board_build.extra_flags","framework","custom_sdkconfig","board_build.arduino.memory_type","board_build.memory_type","board_build.psram_type","board_build.flash_mode","board_upload.flash_size","board_upload.maximum_size","board_build.partitions","build_flags","build_unflags"]
    options={key:env.GetProjectOption(key,None) for key in keys}
    options={key:value for key,value in options.items() if value is not None}
    # The main HTTPS firmware and the USB-only diagnostic intentionally use
    # different SDK allocators. Keep the secure TLS choice gate target-aware.
    main_firmware=(project.resolve()==(repository/"firmware").resolve())
    if main_firmware and "CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y" not in str(options.get("custom_sdkconfig","")):
        raise RuntimeError("Main HTTPS firmware must select external Mbed TLS allocator")
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
    # Inspect the compiled SDK, not sdkconfig.defaults: mutually exclusive
    # Kconfig choices can silently retain their old enabled member.
    def verify_sdk():
        header_path=files[1]
        if not header_path.is_file():
            raise RuntimeError("Verified SDK header missing: re-run managed Core bootstrap")
        header=header_path.read_text(encoding="utf-8")
        flags={name for name in [
            "CONFIG_MBEDTLS_HAVE_TIME_DATE",
            "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE",
            "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL",
            "CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC",
            "CONFIG_SPIRAM",
            "CONFIG_SPIRAM_USE_MALLOC",
        ] if ("#define "+name+" 1") in header}
        required={
            "CONFIG_MBEDTLS_HAVE_TIME_DATE",
            "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE",
            "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL",
        }
        if main_firmware:
            required.update({
                "CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC",
                "CONFIG_SPIRAM",
                "CONFIG_SPIRAM_USE_MALLOC",
            })
        missing=sorted(required-flags)
        forbidden=["CONFIG_ESP_WIFI_DISABLE_KEY_USAGE_CHECK"]
        if main_firmware:
            forbidden.append("CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC")
        forbidden=[name for name in forbidden if ("#define "+name+" 1") in header]
        if missing or forbidden:
            summary="; ".join([
                "missing="+",".join(missing or ["none"]),
                "forbidden_enabled="+",".join(forbidden or ["none"]),
            ])
            raise RuntimeError("Compiled ESP32-S3 SDK config invalid ("+summary+
                "). Main firmware requires PSRAM TLS; standalone diagnostic "+
                "may use internal TLS. CA/date checks always apply. "+
                "Cache will NOT be marked verified.")
    stamp=cache/"verified-sdk.json"
    ready=False
    if stamp.exists() and (cache/"sdkconfig.defaults").exists() and (cache/"sdkconfig.esp32s3").exists():
        try:
            expected=json.loads(stamp.read_text())
            ready=len(expected)==len(files) and hashes()==expected
        except (OSError,ValueError):
            ready=False
    if not ready:
        print("Building the certificate-validating ESP32 SDK in a shared cache without spaces. This first build can take several minutes.",flush=True)
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
        verify_sdk()
        values=hashes()
        if len(values)!=len(files):
            raise RuntimeError("Compiled SDK provenance files are missing")
        stamp.write_text(json.dumps(values,indent=2)+"\n",encoding="utf-8")
    verify_sdk()
    for name in ["sdkconfig.defaults","sdkconfig.esp32s3"]:
        shutil.copyfile(cache/name,project/name)
    allocator="PSRAM TLS" if main_firmware else "standalone default TLS"
    print("PASS: verified custom SDK cache, certificate date checks, "+allocator+" and unchanged application source",flush=True)
