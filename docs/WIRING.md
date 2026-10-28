# Hardware wiring

**English** · [Bahasa Indonesia](id/WIRING.md)

Meter-specific Fort examples below remain valid for Fort. For Acrel, select the saved model first and use [Meter profiles](METER_PROFILES.md) for its register map, terminals, conversion and diagnostic workflow.

The installed circuit uses a manual-direction RS485 transceiver, as confirmed with the successful ModbusMaster sketch. Follow the GPIO labels on the physical ESP32-S3 N16R8 and the signal names on the module because the drawing represents electrical nets rather than physical header positions.

![Complete manual-direction wiring](assets/wiring-overview.svg)

## Supply and shared I2C

With USB disconnected, route 3V3 and GND to separate low-voltage rails and bridge a split breadboard rail if both halves are used. Connect OLED VCC/GND to those rails, SDA to GPIO8 and SCL to GPIO9. Connect DS3231 VCC/GND to the same rails and branch its SDA/SCL onto the same GPIO8/9 nets. The firmware expects SSD1306 128×64 at 0x3C and DS3231 at 0x68, while RTC SQW/32K stay unconnected.

Follow the printed module labels because connector order varies. I2C pull-ups should reference 3.3 V. If the RTC breakout includes a charging circuit, use the battery type specified for that circuit rather than assuming every DS3231 board accepts the same cell. The firmware continues starting its AP and reader when the OLED is absent.

## RS485 and direction control

| From | To | Meaning |
| --- | --- | --- |
| GPIO17 / TX UART1 | DI | ESP32 drives the transceiver input |
| RO | GPIO18 / RX UART1 | Transceiver drives the ESP32 input |
| GPIO16 | DE and /RE joined | HIGH transmits with receiver disabled, LOW receives |
| Board 3V3/GND | Compatible module VCC/GND | Shared local supply/reference |
| A | Fort 58/A | Differential bus conductor |
| B | Fort 59/B | Differential bus conductor |

The Fort port is dedicated to this ESP32 as its only master. Terminal 70/COM is associated with switch inputs and is not an RS485 ground terminal, so it is excluded from the [point-to-point netlist](wiring-netlist.csv). The compatible TTL module and low-voltage circuit are already established project facts.

Select **Manual direction** in Meter setup for this installation. The transmitter is asserted 100 µs before the request and released after UART flush, followed by 100 µs receiver settling. A four-pin [automatic-direction module](assets/rs485-auto.svg) is a separate alternative that requires the Automatic selection and no GPIO16 lead. Do not mix the two diagrams or assume their connector labels have the same physical order.

Use the installation's intended reference, shielding, termination and isolation arrangement. Check resistors already present on the cable/modules before adding bias or termination, and keep this reader connection separate from the meter's voltage/current measurement terminals.

## Tactile switches

![Switch contact pairs](assets/button-contacts.svg)

A four-leg tactile switch has two internally joined contact pairs. Identify them with continuity, then connect one contact from each pair so that pressing joins the GPIO to GND. PREV uses GPIO4, NEXT uses GPIO5 and OK uses GPIO6. There is no button VCC connection because the firmware enables internal pull-ups and debounces the active-LOW input.

## Optional buzzer and onboard LED

The complete drawing includes a two-wire active buzzer with an NPN low-side driver. GPIO7 reaches the base through 1 kΩ, the emitter connects to GND, the collector connects to buzzer negative and buzzer positive connects to its compatible 3V3 supply. A 10 kΩ base-to-ground resistor keeps the transistor off during reset. Those driver parts are additional components, and the transistor package datasheet determines its physical B/C/E pin order.

A driver-equipped buzzer module can use GPIO7 as its signal when its input polarity and supply match the circuit. The firmware output is active HIGH. GPIO48 already drives the onboard addressable RGB LED and needs no external wiring, while OLED rotation is controlled by `OLED_ROTATE_180` in [board.h](../firmware/include/board.h).
