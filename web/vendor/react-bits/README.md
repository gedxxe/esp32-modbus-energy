# React Bits components

These files are copied unchanged from [DavidHDev/react-bits](https://github.com/DavidHDev/react-bits), commit `e12c22192ac7b4b452c5e2feb9b586d74765d696`:

- `FadeContent.jsx`: preserved upstream reference, which requires GSAP/ScrollTrigger. The current dashboard uses a CSS page reveal and does not import this component.
- `SpotlightCard.jsx` and `.css`: pointer/focus lighting on the energy readout.

`upstream.json` records the Git blob SHA for each file. App-specific styling is in `web/styles.css`; the upstream files are not a reusable UI kit supplied by this project. They are included solely as dependencies of the Fort monitor application.

Copyright 2026 David Haz. See [LICENSE.md](LICENSE.md) for MIT + Commons Clause terms. These files are excluded from the project's GPL-3.0-only grant. The browser can read the complete dependency notices at `/assets/third-party.txt` without an internet connection.
