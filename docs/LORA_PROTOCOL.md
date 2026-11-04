# Energy Station radio protocol v1

**English** · [Bahasa Indonesia](id/LORA_PROTOCOL.md) · [Configuration and wiring](LORA.md) · [PTMP and gateway](LORA_PTMP.md)

This is the interoperability contract for the S3 node and a future independently developed ESP32-C6 gateway. It uses raw LoRa with explicit headers, normal IQ, physical packet CRC and a shared 32-byte key. It is not LoRaWAN. The gateway must implement this binary contract rather than treating received bytes as JSON or text.

## Representation and radio defaults

Both ends use the selected carrier, bandwidth, SF, coding denominator, preamble and logical sync byte. Defaults are 915 MHz, 125 kHz, SF7, CR4/5, preamble 8 and sync 0x12. RadioLib's SX126x mapping turns logical 0x12 into register sync 0x1424. All multibyte application integers below are little endian, independent of Modbus register order. Never transmit a native C/C++ struct with padding.

A packet has a 24-byte header, 0–112 payload bytes and a 16-byte tag, for a maximum of 152 bytes. The tag is the first 16 bytes of `HMAC-SHA256(key, header || payload)`. Compare it in constant time before interpreting the frame. The physical LoRa CRC is separate from a capture record's CRC16. Payloads are authenticated but not encrypted.

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 2 | ASCII `ES` |
| 2 | 1 | Protocol version 1 |
| 3 | 1 | HELLO 1, ACK 2, TELEMETRY 3, POLL 4 or CHECKPOINT 5 |
| 4 | 2 | Nonzero network ID |
| 6 | 2 | Source ID, 1–65534 |
| 8 | 2 | Destination ID, 1–65534, different from source |
| 10 | 1 | Exact payload length, at most 112 |
| 11 | 1 | Reserved, zero |
| 12 | 4 | Nonzero sequence |
| 16 | 8 | Nonzero node session nonce |
| 24 | variable | Payload |
| 24 + length | 16 | Truncated HMAC-SHA256 tag |

Wrong version, reserved bits, length, addresses, network, session or authentication cause rejection. The node creates a fresh random 64-bit session on boot, radio reconfiguration or explicit re-pair. Session is not an RTC timestamp. Node sequence increases within the session, which is rotated before sequence wrap. Gateway POLL sequences increase with a signed 32-bit forward distance and restart for a new node session.

## HELLO and ACK

HELLO has 24 payload bytes. Byte 0 is checkpoint schema 3, byte 1 has capability bits 0–2 for live telemetry, checkpoint retrieval and authenticated framing, and bytes 2–23 contain a zero-terminated firmware version with zero padding. The gateway validates HMAC and configured IDs before accepting the session.

An ACK has one payload byte containing the acknowledged type. It swaps source/destination, preserves network and node session, and echoes the exact acknowledged sequence. HELLO ACK therefore contains 1, telemetry ACK 3 and checkpoint ACK 5. An unrelated ACK cannot complete the outstanding packet. A paired session is reported only after the node's own HELLO receives a matching authenticated ACK.

A gateway serving several nodes should keep a separate key for each configured node ID. The gateway should deduplicate `(network, node, session, sequence)` and ACK a valid duplicate again without inserting duplicate data. Node retries retain exact bytes and sequence. A key is never broadcast or negotiated through an unauthenticated message.

## TELEMETRY

The 32-byte payload carries the selected meter's already-scaled engineering kWh. The gateway must not apply CT/PT again.

| Payload offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 4 | Correlated POLL sequence, or 0 for periodic telemetry |
| 4 | 4 | Device UTC reference, 0 when unavailable |
| 8 | 4 | Meter sample age in milliseconds |
| 12 | 1 | Validity flags below |
| 13 | 1 | Immutable meter profile ID |
| 14 | 2 | Saved inventory ID, 0 if unbound |
| 16 | 4 | Current WIB day number |
| 20 | 4 | Current checkpoint revision |
| 24 | 8 | IEEE754 binary64 engineering kWh, little endian |

Flags are bit 0: accepted energy present and no older than 15 seconds, bit 1: valid RTC reference, bit 2: current capture record durable locally, bit 3: meter contact online, and bit 4: diagnostics/discovery/scan active. The kWh slot is absent when bit 0 is clear even if placeholder bytes are zero. Missing data must never become a measured zero. Age includes radio-worker waiting time. UTC is the owner's sampled reference, not browser time.

## Gateway POLL and checkpoint fragments

POLL has five payload bytes. Byte 0 selects0 for live telemetry or 1 for checkpoints, and bytes 1–4 give the WIB day number. Day 0 selects the current capture day. The node accepts polls after pairing, rejects replayed sequences and supports one pending archive query. A busy/rejected poll does not acknowledge a successful read. Use a new increasing poll sequence after a response timeout. Version 1 provides no register writes, RTC changes, reboot or radio-reconfiguration commands.

A live poll returns telemetry with the request sequence in payload bytes 0–3. An archive poll returns CHECKPOINT fragments. Each fragment has a new node sequence and requires ACK before the next fragment. A due or requested live telemetry frame can be interleaved between fragments without ending the archive transfer. The scheduler allows at most one live frame before the next fragment, so neither stream starves when radio pacing exceeds the telemetry interval.

| Fragment payload offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 4 | Correlated POLL sequence |
| 4 | 4 | Actual WIB day number |
| 8 | 2 | Byte offset in reconstructed record |
| 10 | 2 | Total record size, 446 |
| 12 | 1–100 | Contiguous record bytes |

The final fragment is shorter. Validate bounds, overlap, duplicate content, session, poll sequence and day before accepting bytes. One transfer uses a frozen record image. The reconstructed schema 3 record follows `checkpoint_log.h`, including magic, exact size, semantic validation and trailing Modbus-polynomial CRC16, little endian. Captured timestamps/source IDs remain immutable. Old schema 1/2 files are decoded and reserialized as schema 3 without inventing a saved inventory ID.

Validate the full record before database insertion. Use day, revision and captured/source identities for idempotence, not only the radio sequence. ACK confirms packet acceptance, not database durability. The node retains local records regardless of ACK, so the whole day can be requested again after a gateway crash. A missing day currently returns no fragments and increments the node drop counter. Report it as unavailable, never zero or an empty successful record.

## Timing and limits

Only one node packet is outstanding. The radio returns to continuous RX after TX completion and waits for ACK, default 10 seconds. Two default retries permit three total attempts. Random backoff and a shared per-boot 1% airtime gate pace outgoing frames. Since 3.4.1 a matching ACK for an already transmitted outstanding frame is also accepted during retry backoff. ACKs for retired frames, new sessions or an active TX are rejected. The first-TX random contention window is at least 5 seconds for HELLO and 1.5 seconds for other frames, scales with packet airtime and is capped at 30 seconds before the small node-ID offset. The selected delay can be anywhere within that window. Unsent live telemetry is refreshed before its first TX. A live packet older than 15 seconds is dropped rather than retried as fresh, while record fragments remain valid dated data. At slow settings, the gate can exceed the ACK timeout. Retries wait for that budget rather than transmitting immediately. The gateway must schedule compatible responses across its own half-duplex link and multiple nodes. POLL deadlines must cover contention, duty and owner-service waits rather than using the node ACK timeout as a response deadline.

After retry exhaustion the node becomes unpaired and makes a later HELLO attempt. It does not reset the ESP32 or erase measurements. Live telemetry coalesces to the latest sample. Archive/control queues are bounded. Core 1 reads archives outside urgent capture admission, while core 0 owns SPI/radio. Gateway request rates must respect these limits.

The [public software vector](lora-v1-vector.json) contains a HELLO body, test key, tag and complete packet. Never use that public key in an installation. It supports independent gateway codec/HMAC checks and is not RF evidence. Gateway firmware, database durability, range and physical pairing remain separate acceptance work.
