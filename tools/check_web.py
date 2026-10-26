"""Check the committed browser bundle without Node or third-party Python packages."""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parent.parent

def check():
    manifest = json.loads((ROOT / "web/bundle.json").read_text())
    expected = {p.relative_to(ROOT).as_posix() for p in (ROOT / "web").rglob("*") if p.is_file() and p.name != "bundle.json"}
    expected.update(["tools/build-web.cjs", "package.json", "package-lock.json"])
    if expected != set(manifest["inputs"]):
        raise SystemExit("Web input list changed. Run npm ci && npm run build:web.")
    for name, digest in manifest["inputs"].items():
        if hashlib.sha256((ROOT / name).read_text(encoding="utf-8").encode("utf-8")).hexdigest() != digest:
            raise SystemExit("Stale web bundle: " + name + "; run npm run build:web.")
    if hashlib.sha256((ROOT / "firmware/include/web_ui.h").read_bytes()).hexdigest() != manifest["header_sha256"]:
        raise SystemExit("Generated web header changed; run npm run build:web.")
    if manifest["compressed_total"] > 160 * 1024:
        raise SystemExit("Web bundle exceeds its compressed size budget.")
    print("PASS: web source hashes and generated header; " + str(manifest["compressed_total"]) + " bytes gzip")

if __name__ == "__main__":
    check()
