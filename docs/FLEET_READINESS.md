# Fleet deployment and firmware readiness

**English** · [Bahasa Indonesia](id/FLEET_READINESS.md) · [Validation](VALIDATION.md) · [Power interruption](POWER_LOSS.md)

Version 3.5.0 is a candidate for controlled deployment on ESP32-S3 N16R8 units with the documented pinout. Software checks can establish bounded behavior and recovery rules, but they do not establish regulator stability, RF capacity, meter accuracy or a lifetime failure rate. Approve a small pilot before treating one firmware image as accepted across an installation.

## Findings and implemented behavior

| Finding | Implemented behavior | Remaining acceptance |
| --- | --- | --- |
| An older staging file could replace a newer valid archive | Compare version and immutable captured points before promotion. Discard staging only when a validated existing copy covers it. Preserve conflicting records and stop that promotion. | Interrupt actual flash operations and compare exported records |
| Meter and Wi-Fi configuration had one application record | Alternate explicit CRC/generation banks with exact readback. Retain the previous verified bank when the next write fails. Expose degraded bank status. | Power-cut tests during migration and configuration updates |
| A new unit could read using an unconfirmed Fort default | Recording waits for an explicit meter configuration save. Web, OLED, discovery and diagnostics stay available. Existing valid settings are migrated. | Verify the selected profile and LCD value on every unit |
| Identical images shared the same AP name and hostname | Derive a stable public Device ID, AP name and hostname from the complete factory MAC. | Compare the displayed ID with the commissioning inventory |
| Wi-Fi could process an event received beyond its attempt deadline | Judge admission by the event timestamp and stop an expired attempt. A recovered RTC cannot repeatedly restart preparation while it is already active. | Correct/incorrect login, DHCP delay and signal-loss tests |
| Millisecond uptime wrapped after about 49 days | Use the 64-bit ESP timer for reported uptime and initial buzzer grace. Existing short elapsed timers keep unsigned rollover arithmetic. | A sustained soak under realistic traffic |
| Radio telemetry used some fields from the previous publish | Construct age, contact and energy flags from the same core-1 publication. | Compare live RF metadata against node status |

Meter maps, profile IDs, checkpoint schema 3, Energy Station v1, pinout and partition offsets are unchanged. The separate gateway must still implement and validate that radio contract.

## Identifying each unit

The default AP name is now `Midea Engineering-XXXXXXXXXXXX`, where the twelve hexadecimal characters are the complete factory MAC used as Device ID. The hostname is `energy-xxxxxxxxxxxx`. These values are derived at boot rather than copied from NVS, so flashing one build onto different boards does not copy their identity. The AP password remains the configured value in `board.h`.

Read the full name on the OLED Network page or its AP password menu, and compare Device ID on **Device & records → This unit**. The browser also shows meter/Wi-Fi configuration generations and warnings about invalid setting banks. Identity is public installation metadata, not an authentication secret. A failed identity read leaves a visible unavailable state and fallback AP name, and that unit should not pass commissioning.

`AP_UNIQUE_NAME=false` deliberately retains the earlier `Midea Engineering AP` name for a single-unit installation. Keep the default enabled for a fleet. Changing this setting does not change the Modbus address or LoRa Node ID.

## First commissioning and updates

Flash the factory image only when provisioning a blank unit. For an installed logger, use PlatformIO Upload or the app image at `0x10000` with the unchanged partition layout. The merged factory image includes padding across NVS and is not the normal update path for retaining settings. Follow [flashing instructions](FLASHING.md).

A blank unit may need LittleFS initialization because the factory image does not contain an initialized filesystem. Follow the documented physical OK-at-boot confirmation and five-second hold only when erasing that filesystem is intended. This procedure removes filesystem archives on an existing unit. It is never performed automatically.

On a fresh unit, join its displayed AP and open its displayed address, normally `http://192.168.4.1`. In **Meter setup**, choose the installed model, match its slave ID/baud/parity and manual direction, then save. The former implicit Fort default no longer authorizes production. ACR/APM retain their additional verified saved-device requirement. Check the live cumulative kWh against the meter LCD, verify RTC/UTC and WIB presentation, and confirm storage status before leaving.

Give every radio station its own Node ID and pairing key, matching the gateway inventory. Network ID and Gateway ID describe the intended network/receiver and may be shared. Do not enable many stations with the default Node ID 2. Device ID is not transmitted as a new field in protocol v1 and does not automatically allocate a radio address.

Do not clone a full flash dump from a commissioned unit onto another unit. Such a dump can contain its Wi-Fi credentials, radio ID/key, saved-meter inventory and historical readings. Use the built firmware images and commission each unit separately. Keep credentials outside a public commissioning worksheet.

For each unit, retain Device ID, asset/location, board revision, firmware version and image hash, meter model/profile, serial settings, Node/Network/Gateway IDs if used, reference LCD value, clock/storage checks, operator/date and acceptance result. Preserve historical records under their original source identities.

## Configuration recovery format

The 16-byte meter payload is stored in 30-byte envelopes at `config_a` and `config_b`. The version-3 Wi-Fi payload remains 779 bytes inside 793-byte envelopes at `wifi_a` and `wifi_b`. Each envelope contains magic `SBK1`, version 1, a reserved zero byte, explicit payload length, a nonzero 32-bit generation, payload and CRC16. Serialization is independent of struct padding.

Core 1 exclusively reads/writes these Preferences records. A save targets the inactive bank, compares the exact readback, and changes the active RAM settings only after verification. An unsuccessful meter save leaves the current verified production configuration running. The first saved configuration has one confirmed bank; subsequent saves retain a previous confirmed version. This is recovery from interrupted updates, not a claim that every initial record already has two identical physical copies.

Legacy `config` and 104/108/779-byte `wifi` records are imported only when no new bank exists. Legacy keys are retained, but once any bank exists a damaged journal cannot silently fall back to stale legacy values. If neither bank validates, meter production pauses or Wi-Fi uses recovery AP defaults. An interrupted initial migration can therefore require the operator to confirm settings again. Conflicting equal-generation banks also require review. A valid older bank can recover service, but an invalid alternate copy is shown as degraded until repaired by a verified save.

Older firmware does not understand these bank keys and legacy keys are no longer updated. A downgrade can load an older legacy configuration, so export records and re-confirm all settings when deliberately downgrading. The NVS partition stays 20 KiB. Capacity is finite, and a failed save is reported rather than solved by formatting or changing partition offsets.

## Pilot acceptance

Start with one bench unit, then a small group before expanding. Check boot without USB Serial, absent OLED/RTC/radio, meter unplug/reconnect, no-response/CRC faults, correct and incorrect Wi-Fi login, DHCP failure, storage degradation and recovery. Repeat power interruption during settings saves and around a scheduled checkpoint, using a dedicated test unit and retained exports. A restart must restore a complete valid saved version or remain unavailable, never fabricate a captured point.

Run a 24–72 hour soak with representative browser traffic, Wi-Fi reconnects and radio work, observing minimum heap, largest free block, task stack watermarks, watchdog/reset reasons and storage status. These durations are a proposed acceptance procedure, not tests already completed. Longer electrical/endurance testing depends on the installation's reliability requirement.

For LoRa, test one gateway/node pair before adding three independently addressed nodes. Check duplicate IDs, separate keys, loss/retry behavior, checkpoint transfers and receiver restart. A single SX1262 and the existing 1% per-radio pacing gate do not support an arbitrary node count at the default interval. Use [PTMP airtime planning](LORA_PTMP.md) and record actual gateway/RF acceptance separately.

A stable supply, RTC battery, RS485 installation and radio supply/antenna remain part of the complete system. NVS and LittleFS share the flash chip. Neither CRC, software retries nor an ACK can recover a measurement if every saved copy is lost.
