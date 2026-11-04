#!/usr/bin/env python3
"""Fail CI on high-confidence secrets in tracked repository files.

Uses only Python standard library. Never prints detected secret values.
This is a safety net, not a substitute for verified external scanning,
historical commit review, binary image inspection, or credential rotation.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import PurePosixPath

TOKEN_PATTERNS = (
    ("private_key", re.compile(rb"-{5}BEGIN (?:RSA |OPENSSH |EC |DSA |ENCRYPTED )?PRIVATE KEY-{5}")),
    ("github_token", re.compile(rb"\b(?:gh[pousr]_[A-Za-z0-9_]{30,}|github_pat_[A-Za-z0-9_]{30,})\b")),
    ("aws_access_key", re.compile(rb"\b(?:AKIA|ASIA)[0-9A-Z]{16}\b")),
    ("google_api_key", re.compile(rb"\bAIza[0-9A-Za-z_-]{30,}\b")),
    ("openai_key", re.compile(rb"\bsk-[A-Za-z0-9_-]{25,}\b")),
    ("slack_token", re.compile(rb"\bxox[baprs]-[A-Za-z0-9-]{15,}\b")),
    ("bearer_literal", re.compile(rb"\bAuthorization:\s*(?:Bearer|Basic)\s+[A-Za-z0-9+/=_-]{20,}", re.I)),
    ("url_password", re.compile(rb"https?://[A-Za-z0-9_.-]+:[^@\s/]{5,}@[A-Za-z0-9.-]+", re.I)),
)
# These are forbidden in normal code/docs. In unit tests, obvious test-only
# fixtures remain permitted; actual provider-formatted tokens are never allowed.
ASSIGNED_SECRET = re.compile(
    rb"\b(?:AP_PASSWORD|WIFI_PASSWORD|CLIENT_SECRET|API_KEY|ACCESS_TOKEN|REFRESH_TOKEN|"
    rb"PASSWORD|PASSWD|PASSPHRASE|PSK)\b\s*(?:\[[^\]\r\n]*\])?\s*[:=]\s*"
    rb"['\"]([^'\"\r\n]{8,})['\"]", re.I
)
MARKDOWN_PASSWORD = re.compile(
    rb"\b(?:password|kata sandi)\b[^\r\n]{0,32}\*{2}([A-Za-z0-9!@#$%&*_+-]{8,64})\*{2}", re.I
)
PATH_BLOCKED = re.compile(
    r"(?i)(?:^|/)(?:\.env(?:\.[^/]+)?|\.netrc|id_rsa|id_ed25519|"
    r"credentials(?:\.[^/]+)?|secrets(?:\.[^/]+)?|\.npmrc)$|"
    r"\.(?:pem|key|p12|pfx|jks)$"
)
BINARY_SUFFIXES = {".png", ".jpg", ".jpeg", ".gif", ".webp", ".mp4", ".pdf", ".rle", ".xbm"}

def scan_bytes(path: str, content: bytes) -> list[tuple[str, int]]:
    findings = []
    for label, pattern in TOKEN_PATTERNS:
        for match in pattern.finditer(content):
            findings.append((label, content.count(b"\n", 0, match.start()) + 1))
    is_binary = PurePosixPath(path).suffix.lower() in BINARY_SUFFIXES or b"\0" in content[:1024]
    if not is_binary:
        is_fixture = path.startswith("firmware/test/") or path.startswith("tools/test-")
        if not is_fixture:
            for label, pattern in (("embedded_credential", ASSIGNED_SECRET),
                                   ("documented_password", MARKDOWN_PASSWORD)):
                for match in pattern.finditer(content):
                    findings.append((label, content.count(b"\n", 0, match.start()) + 1))
    return findings

def tracked_paths() -> list[str]:
    raw = subprocess.check_output(["git", "ls-files", "-z"])
    return [p.decode("utf-8") for p in raw.split(b"\0") if p]

def self_test() -> None:
    demo = b'constexpr char AP_' + b'PASSWORD[] = "' + b'demo-only-secret-123";'
    assert scan_bytes("firmware/include/config.h", demo)
    assert not scan_bytes("firmware/test/fixtures.cpp", demo)
    assert not scan_bytes("README.md", b"Use the per-device password shown on the OLED.")
    assert PATH_BLOCKED.search(".env.production")
    assert PATH_BLOCKED.search("configs/id_rsa")
    print("PASS: tracked-file security scanner self-test")

def main() -> int:
    if "--self-test" in sys.argv:
        self_test()
        return 0
    paths = tracked_paths()
    errors = []
    binary = 0
    for path in paths:
        if PATH_BLOCKED.search(path) and not path.endswith(".example"):
            errors.append((path, 1, "sensitive_filename"))
        try:
            with open(path, "rb") as handle:
                content = handle.read()
        except OSError:
            errors.append((path, 1, "unreadable"))
            continue
        if PurePosixPath(path).suffix.lower() in BINARY_SUFFIXES or b"\0" in content[:1024]:
            binary += 1
        for category, line in scan_bytes(path, content):
            errors.append((path, line, category))
    for path, line, category in errors[:80]:
        # Deliberately no value/context: CI logs must not amplify exposure.
        print(f"SECURITY AUDIT: {path}:{line}: {category}", file=sys.stderr)
    if len(errors) > 80:
        print(f"SECURITY AUDIT: {len(errors)-80} more matches suppressed", file=sys.stderr)
    print(f"Tracked files scanned: {len(paths)} (including {binary} binary/asset files)")
    if errors:
        print(f"FAILED: {len(errors)} potential secrets or prohibited filenames; review locally.", file=sys.stderr)
        return 1
    print("PASS: no high-confidence exposed credentials in tracked current-tree content")
    print("Limits: binary images/PDF streams, historical Git objects and runtime storage are not fully inspected.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
