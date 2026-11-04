#!/usr/bin/env python3
"""Zero-trust repository hygiene gate: tracked files and reachable Git history.

No secret values are ever printed. Known factory recovery AP password is
an explicitly published operational default, not a confidential credential.
"""
from __future__ import annotations

import hashlib
import re
import shutil
import subprocess
import sys
from pathlib import Path

PUBLIC_AP_SHA256 = "1f4890dc282f23771dadf96e7f9d6838b9ed5712b88e41d0544afb833ff5d697"
AP_SOURCE = "firmware/include/board.h"
AP_DOCS = frozenset({
    "README.md", "README.id.md", "docs/WEB.md", "docs/id/WEB.md",
    "docs/CHECKPOINTS.md", "docs/id/CHECKPOINTS.md",
    "docs/DIAGNOSTICS.md", "docs/id/DIAGNOSTICS.md",
})
BLOCKED_PATH = re.compile(
    r"(?i)(^|/)(?:\.env(?:\.[^/]+)?|\.netrc|\.npmrc|id_rsa|id_ed25519|"
    r"credentials(?:\.[^/]+)?|secrets(?:\.[^/]+)?)(?:$|/)|"
    r"\.(?:key|p12|pfx|jks)$"
)
DETECT = [
    ("private_key", re.compile(rb"-----BEGIN (?:RSA |EC |DSA |OPENSSH |ENCRYPTED )?PRIVATE KEY-----")),
    ("github_token", re.compile(rb"\b(?:gh[pousr]_[A-Za-z0-9_]{30,}|github_pat_[A-Za-z0-9_]{30,})\b")),
    ("aws_access_key", re.compile(rb"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b")),
    ("google_api_key", re.compile(rb"\bAIza[A-Za-z0-9_-]{30,}\b")),
    ("openai_api_key", re.compile(rb"\bsk-[A-Za-z0-9_-]{28,}\b")),
    ("slack_token", re.compile(rb"\bxox[baprs]-[A-Za-z0-9-]{16,}\b")),
    ("bearer_basic", re.compile(rb"\bAuthorization\s*:\s*(?:Bearer|Basic)\s+[A-Za-z0-9+/_=-]{18,}", re.I)),
    ("embedded_db_password", re.compile(rb"\b(?:postgres(?:ql)?|mysql|mongodb(?:\+srv)?)://[^:\s/@]+:[^@\s/]{6,}@", re.I)),
]
# Restrict the static-literal check to source-like lines, rather than translated
# UI labels such as {"password": "Enter your password"}.
STATIC_SECRET = re.compile(
    rb"\b(?:AP_PASSWORD|WIFI_PASSWORD|WIFI_PSK|PASSWORD|PASSWD|"
    rb"CLIENT_SECRET|API_KEY|ACCESS_TOKEN|REFRESH_TOKEN|SERVICE_TOKEN)\b"
    rb"\s*(?:\[[^\]\r\n]*\])?\s*=\s*[\"']([^\"'\r\n]{8,})[\"']", re.I
)
MARKDOWN_PASSWORD = re.compile(
    rb"\b(?:password|kata sandi)\b[^\r\n]{0,20}\*{2}([A-Za-z0-9!@#$%&*_+.-]{8,64})\*{2}", re.I
)
MAX_REPORTS = 80
PDF_LIMIT = 9_000_000
MAX_GIT_OBJECT = 10_000_000


def git(*args: str) -> bytes:
    return subprocess.check_output(["git", *args], stderr=subprocess.DEVNULL)


def approved_public_ap(path: str, value: bytes) -> bool:
    return path == AP_SOURCE and hashlib.sha256(value).hexdigest() == PUBLIC_AP_SHA256


def approved_public_doc(path: str, value: bytes) -> bool:
    return path in AP_DOCS and hashlib.sha256(value).hexdigest() == PUBLIC_AP_SHA256


def scan_bytes(path: str, data: bytes) -> list[tuple[str, int]]:
    findings: list[tuple[str, int]] = []
    for kind, pattern in DETECT:
        for match in pattern.finditer(data):
            findings.append((kind, data.count(b"\n", 0, match.start()) + 1))
    if b"\0" in data[:1024]:
        return findings
    is_fixture = path.startswith("firmware/test/") or path.startswith("tools/test-")
    if not is_fixture:
        for match in STATIC_SECRET.finditer(data):
            if not approved_public_ap(path, match.group(1)):
                findings.append(("hardcoded_secret", data.count(b"\n", 0, match.start()) + 1))
        for match in MARKDOWN_PASSWORD.finditer(data):
            if not approved_public_doc(path, match.group(1)):
                findings.append(("documented_password", data.count(b"\n", 0, match.start()) + 1))
    return findings


def get_pdf_text(path: str, data: bytes) -> bytes | None:
    if not shutil.which("pdftotext") or len(data) > PDF_LIMIT:
        return None
    try:
        result = subprocess.run(["pdftotext", "-layout", "-", "-"], input=data,
                                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                timeout=20, check=False)
    except (OSError, subprocess.TimeoutExpired):
        return None
    return result.stdout if result.returncode == 0 else None


def assert_default_recovery() -> None:
    board = Path(AP_SOURCE).read_bytes()
    m = STATIC_SECRET.search(board)
    assert m and approved_public_ap(AP_SOURCE, m.group(1)), (
        "Factory AP default changed or removed. Preserve headless recovery."
    )
    main = Path("firmware/src/main.cpp").read_text(encoding="utf-8")
    assert "WiFi.softAP(unitIdentity.apSSID,AP_PASSWORD" in main, (
        "Production AP must use published default when OLED is unavailable."
    )
    assert "provisionAPCredential(" not in main, (
        "Random-only recovery password breaks OLED-less commissioning."
    )
    print("PASS: factory AP recovery password and OLED-independent code path unchanged")


def self_test() -> None:
    test = b'constexpr char CLIENT_SECRET[] = "non-public-literal";'
    assert scan_bytes("firmware/include/config.h", test)
    assert not scan_bytes("firmware/test/fixture.cpp", test)
    assert scan_bytes("README.md", b"Default password **a-different-password**")
    assert not scan_bytes("README.md", b"To join AP use the documented recovery password.")
    assert BLOCKED_PATH.search(".env.production")
    assert BLOCKED_PATH.search("secrets/config.json")
    assert not BLOCKED_PATH.search("examples/README.md")
    assert_default_recovery()
    print("PASS: security audit fixtures")


def run() -> int:
    if "--self-test" in sys.argv:
        self_test()
        return 0
    if "--history" not in sys.argv and "--current" not in sys.argv:
        print("USAGE: python tools/security_audit.py --current [--history]")
        return 2
    assert_default_recovery()
    violations: list[tuple[str, str, int]] = []
    scanned = 0
    pdf_extracted = 0
    opaque = 0

    def review(path: str, data: bytes, *, historical: bool = False) -> None:
        nonlocal scanned, pdf_extracted, opaque
        scanned += 1
        if not historical and BLOCKED_PATH.search(path):
            violations.append((path, "sensitive_filename", 0))
        for kind, line in scan_bytes(path, data):
            violations.append((path, kind, line))
        if path.lower().endswith(".pdf"):
            extracted = get_pdf_text(path, data)
            if extracted is None:
                opaque += 1
            else:
                pdf_extracted += 1
                for kind, line in scan_bytes(path, extracted):
                    violations.append((path, f"pdf_text_{kind}", line))

    current_paths = [p.decode("utf-8") for p in git("ls-files", "-z").split(b"\0") if p]
    for path in current_paths:
        review(path, Path(path).read_bytes())

    historical_blobs = 0
    if "--history" in sys.argv:
        # Reachable Git history, not reflogs, orphaned/unreachable objects or forks.
        # A full checkout is required; CI uses fetch-depth: 0.
        seen: set[str] = set()
        for line in git("rev-list", "--objects", "--all").splitlines():
            sha, _, path_raw = line.partition(b" ")
            key = sha.decode()
            if key in seen or git("cat-file", "-t", key).strip() != b"blob":
                continue
            seen.add(key)
            size = int(git("cat-file", "-s", key))
            path = path_raw.decode("utf-8", "replace")
            if size > MAX_GIT_OBJECT:
                opaque += 1
                continue
            review(path or "[historical unknown path]", git("cat-file", "blob", key),
                   historical=True)
            historical_blobs += 1

    for path, kind, line in violations[:MAX_REPORTS]:
        # Never echo actual values or excerpts into logs.
        print(f"SECURITY: {path}:{line}: {kind}", file=sys.stderr)
    if len(violations) > MAX_REPORTS:
        print(f"SECURITY: {len(violations)-MAX_REPORTS} additional hits omitted",
              file=sys.stderr)
    print(f"Scanned {len(current_paths)} current files; {historical_blobs} historic blobs; "
          f"{pdf_extracted} PDF text streams extracted; {opaque} opaque/skipped assets")
    if violations:
        print(f"FAILED: {len(violations)} potential credentials or sensitive paths.",
              file=sys.stderr)
        return 1
    print("PASS: no matching unapproved credentials in inspected content")
    print("NOTE: pattern scans do not prove all screenshots, compressed data, "
          "unreachable Git objects or deployed NVS are clean.")
    return 0


if __name__ == "__main__":
    raise SystemExit(run())
