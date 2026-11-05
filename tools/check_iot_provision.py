"""Sanitized, build-time verification of optional local IoT provisioning.

This file never prints the bearer or injects any credential into compiler flags.
"""
from pathlib import Path
import re

ID = re.compile(r"[A-Za-z0-9][A-Za-z0-9_-]{0,63}\Z")
KEY = re.compile(r"[A-Za-z0-9_-]{43}\Z")
ENDPOINT = re.compile(r"https://[A-Za-z0-9.-]+(?::443)?/api/iot-ingest\Z")

def validate_header(path):
    path = Path(path)
    if not path.is_file():
        return False
    # Parse the simple header emitted by the website; never echo its contents.
    lines = path.read_text(encoding="utf-8").splitlines()
    macros = {}
    for line in lines:
        match = re.match(r'^\s*#define\s+(IOT_DEV_[A-Z_]+)\s+(.*?)\s*$', line)
        if not match:
            continue
        name, value = match.groups()
        if name in macros:
            raise ValueError("duplicate " + name + " macro")
        macros[name] = value
    enabled = macros.get("IOT_DEV_ENABLED")
    if enabled not in ("0", "1"):
        raise ValueError("IOT_DEV_ENABLED must be literal 0 or 1")
    if enabled == "0":
        return True
    expected = (
        "IOT_DEV_ENDPOINT", "IOT_DEV_DEVICE_ID", "IOT_DEV_METER_ID",
        "IOT_DEV_METER_PROFILE", "IOT_DEV_BEARER",
    )
    values = {}
    for key in expected:
        quoted = re.fullmatch(r'"([^"\r\n]*)"', macros.get(key, ""))
        if not quoted:
            raise ValueError(key + " must be a single quoted string macro")
        values[key] = quoted.group(1)
    if not ENDPOINT.fullmatch(values["IOT_DEV_ENDPOINT"]):
        raise ValueError("IoT endpoint must be strict HTTPS ending /api/iot-ingest")
    for key in ("IOT_DEV_DEVICE_ID", "IOT_DEV_METER_ID", "IOT_DEV_METER_PROFILE"):
        if not ID.fullmatch(values[key]):
            raise ValueError(key + " has an invalid identifier")
    if not KEY.fullmatch(values["IOT_DEV_BEARER"]):
        raise ValueError("IoT bearer must be 43 URL-safe characters")
    return True

def self_test():
    from tempfile import TemporaryDirectory
    def header(bearer, endpoint="https://synthetic.invalid/api/iot-ingest", enabled="1"):
        return (
            "#pragma once\n"
            f"#define IOT_DEV_ENABLED {enabled}\n"
            f'#define IOT_DEV_ENDPOINT "{endpoint}"\n'
            '#define IOT_DEV_DEVICE_ID "synthetic_01"\n'
            '#define IOT_DEV_METER_ID "synthetic_meter"\n'
            '#define IOT_DEV_METER_PROFILE "fort"\n'
            f'#define IOT_DEV_BEARER "{bearer}"\n'
        )
    with TemporaryDirectory() as directory:
        private = Path(directory) / "iot_credentials.local.h"
        assert not validate_header(private)
        private.write_text(header("A"*43), encoding="utf-8")
        assert validate_header(private)
        for value in ("short", "A"*42+"!", "A"*44):
            private.write_text(header(value), encoding="utf-8")
            try:
                validate_header(private)
                raise AssertionError("bad key accepted")
            except ValueError:
                pass
        private.write_text(header("A"*43, "http://synthetic.invalid/api/iot-ingest"), encoding="utf-8")
        try:
            validate_header(private)
            raise AssertionError("HTTP endpoint accepted")
        except ValueError:
            pass
        private.write_text(header("", enabled="0"), encoding="utf-8")
        assert validate_header(private)
    print("PASS: local IoT provisioning checks; no secrets emitted")

if __name__ == "__main__":
    self_test()
