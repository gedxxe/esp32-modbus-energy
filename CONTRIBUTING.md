# Contributing

**English** · [Bahasa Indonesia](CONTRIBUTING.id.md)

Read [AGENTS.md](AGENTS.md) for confirmed hardware and ownership rules. Keep changes to the dedicated Fort reader; do not add meter writes or alter the register map to work around a wiring fault.

## Development tools

- PlatformIO Core 6.1.16 (`python -m pip install -r requirements-dev.txt`).
- g++ with C++11 support for native tests.
- Python 3.10+ for SVG generation, source checks and packaging.
- Node 24.22.1 and `npm ci` when editing/testing the browser app.

The device build uses a committed generated browser header. `pio run` checks it using Python without installing Node. For browser changes, edit `web/Application.jsx`, the views in `web/views/`, `web/i18n.js` or `web/styles.css`, then run `npm run build:web`. Do not hand-edit the generated header. `npm run check:web` rebuilds in memory and compares generated output; the PlatformIO guard checks source/header hashes.

## Checks

```powershell
.\tools\test.ps1
python tools/generate_docs.py
python tools/check_docs.py
npm run check:web
npm run test:ui
cd firmware
pio run -e esp32s3
```

On Linux/macOS, replace the native-test command with:

```sh
mkdir -p tmp
g++ -std=c++11 -Wall -Wextra -Werror -Ifirmware/include firmware/test/test_energy.cpp -o tmp/fort-test
./tmp/fort-test
```

Install Chromium using `npx playwright install chromium`, or use an installed Edge with `node tools/preview-test.cjs playwright msedge`. The browser test serves the exact gzip arrays from `web_ui.h` with a simulated API. It checks EN/ID behavior, scan controls, exports, request failures, responsive widths, reduced motion and external requests. It also verifies vendored React Bits blob hashes and renders the SVG diagrams. Screenshots are simulated data, not hardware acceptance evidence.

## Documentation

English is the primary language. Keep [README.id.md](README.id.md) and `docs/id/` usable alongside their English counterparts. Use short operational sentences and explain implementation limits where they affect decisions. SVG text/layout belongs in `tools/generate_docs.py`; generated files must be reproducible and readable at the intended size. Preserve pin labels, Fort 58/59 and the distinction from switch-input 70/COM.

Changes to frame parsing, interval quality, recovery or debounce need meaningful native regression coverage. Changes to visible actions or language behavior need browser coverage. Update [VALIDATION](docs/VALIDATION.md) with results actually obtained, including the difference between software tests and physical commissioning.

## Version and package

Keep `version.h`, `package.json`, lockfile, badges and changelog aligned. A package uses the verified N16R8 hardware build:

1. Compile after the last web/firmware edit.
2. Copy the current hardware app, bootloader and partition BINs into `output/firmware/` and merge the factory image using the [documented offsets](docs/FLASHING.md).
3. Run `python tools/package.py`.
4. Check the manifest SHA-256 entries and ZIP integrity. Include web source, dependency notices, guides and diagrams; exclude build caches and installed dependencies.

`output/`, `.pio/`, `node_modules/` and temporary files are ignored by Git. Local build caches and installed dependencies may be removed after verification; PlatformIO restores its libraries/build output, and `npm ci` restores web development dependencies. Keep the committed browser header/manifest and documentation assets. Source publication and release-asset publication are separate operations. Do not substitute a binary-only ZIP for source.

## Third-party code

Original project code is GPL-3.0-only. `web/vendor/react-bits/` retains its own MIT + Commons Clause terms; do not describe it as GPL or as plain MIT. Upstream files are pinned in `upstream.json` and included only as dependencies of this monitor application. React/React DOM and GSAP retain their own notices. Update [NOTICE](NOTICE) and offline `/assets/third-party.txt` whenever shipped dependencies change. The Fort PDF is a third-party reference and is not relicensed.
