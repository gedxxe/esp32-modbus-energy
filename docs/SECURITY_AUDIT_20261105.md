# Repository audit notes (2026-11-05)

This audit is based on main commit 770512260ef5133296790ee21d2635c55f00196f.

## Headless recovery contract

The published factory access-point setting is intentionally retained in
`firmware/include/board.h`. A failed or absent OLED cannot be allowed to
block local maintenance. The normal firmware starts the AP directly from
that configuration. Keep the recovery network isolated; the local web
application uses HTTP rather than TLS or individual user accounts.

## Repeatable verification

Run `python tools/security_audit.py --self-test` and
`python tools/security_audit.py --current --history`.

The gate inspects tracked source and docs, plus reachable Git blob history.
It reports categories and paths, not matching values. PDFs are text
extracted where supported. Binary screenshots, inaccessible clones, other
branches not fetched, and physical device storage are outside a complete
text scan.

## ADL400 decoding crosscheck

Production meter profile `acrel-adl400` is ID 5. The independently
tested unit used Modbus RTU FC03 at slave ID 220, 9600 baud and 8N1.

| Quantity | Register | Words |
| --- | --- | --- |
| Total active energy | 0x0000 | 2 |
| PT | 0x008D | 1 |
| CT | 0x008E | 1 |

UInt32 ABCD raw value 9798, scaled by 0.01, then PT 1 and CT 80 gives
7838.40 kWh. The reported meter LCD was approximately 7838.9 kWh,
a difference below one 0.8 kWh scaled raw step. This is consistent with
the field reading but does not establish full calibration accuracy.

The normal firmware uses the shared FC03 transport, independent PT and
CT reads, quality validation and primary-energy scaling. Host tests
replay the observed frame through normal reading helpers without requiring
an interactive diagnostic command. Physical end-to-end confirmation on
the installed ADL400 remains necessary.

## Maintainability boundaries

The protocol transport and reading profiles are modular. The large
`firmware/src/main.cpp` remains a maintainability concern; a broad
refactor should be separate from this audit to avoid changes in storage
schemas, core ownership, UART timing and recovery behavior.
