# PrecisionShot firmware

Native C++ firmware for the ESP32-S3 and Hosyond ST7796S/FT6336 touchscreen.
Build with **ESP-IDF 6.0.2**. Only the official SDK and standard C/C++ are
dependencies; no Arduino, third-party firmware libraries, graphics framework,
or managed components are used. Component Manager is disabled by CMake.

## Build and flash

From an ESP-IDF 6.0.2 terminal in this repository:

```text
idf.py set-target esp32s3
idf.py build
idf.py -p COM6 flash
idf.py -p COM6 monitor
```

Use the device's actual port if Windows assigns a different one. Exit the
monitor with Ctrl+]. The console uses the ESP32-S3's native USB Serial/JTAG
port at 115200 baud. The defaults select 8 MB flash, DIO, one factory app,
and an 8 KB main-task stack. CPU speed is 240 MHz. Two universal MAC addresses
preserve the previous Bluetooth identity (factory base MAC + 1). PSRAM is not
required by this prototype. The attached development board reports 16 MB of
physical flash; the firmware intentionally uses the 8 MB project configuration.
Flashing replaces the bootloader, partition table, and application; it does
not run a whole-flash erase. Do not run a flash erase to fix an NVS error
without first identifying whether stored data needs to be retained.

On this Windows workstation, activate the installed SDK with:

```powershell
. C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1
```

## Training screens and controls

- **Freestyle:** unlimited shots; normal and fullscreen views show the last score.
- **Classic:** counts down from 10 shots. Shot 10 leaves the total out of 100 on
  screen. The next incoming shot starts a new round and counts as shot 1.
- **Fullscreen:** mode title at top left, close at top right, large centered
  score. Classic shows shots remaining at bottom left until the round completes.
- **DEBUG +HIT:** bottom-right button in both normal and fullscreen training
  views, and in Session Debug. Generates a simulated incoming score cycling
  10, 9, 8, 7. A held finger generates only one hit until released.
- **Menu:** eased slide animation for Freestyle, Classic, Settings and Session
  Debug. Rapid is removed from the menu.
- **Settings:** distance from 1 to 100 meters (or equivalent feet), unit toggle,
  calibration sensitivity from 0 to 4095 in steps of 100, theme toggle, and BLE
  advertising restart. Higher sensitivity-threshold values mean less sensitive
  detection. Unit toggles preserve the stored physical distance.
- **Session Debug:** tabs show the exact latest shot payload as JSON text, hex,
  or binary bytes, including offline previews. A metadata row records the mode,
  shot number, total, distance and sensitivity at the time of that test shot.
  Delivery status distinguishes offline, unsubscribed, queued and failed;
  queued means accepted by the SDK, not acknowledged by the phone.

- **Settings > Debug Zone:** four animation buttons (Confetti Chaos, Cosmic
  Orbit, Jelly Bounce, Warp Speed) preview `HIGHSCORE XXX` for five seconds,
  then return to the picker. Close cancels playback; close again returns to
  Settings. These are visual demos and do not record shots or change scores.
  Animation timing is nonblocking so the main loop continues servicing touch
  and Bluetooth. The bottom bar shows time remaining.

The initial debug packet is explicitly an unsent example. Physical laser
scanning and ambient-baseline calibration are not connected yet; the adjustable
threshold is configuration for that future detection path. Distance and
calibration controls work locally and through the commands below. Settings,
themes, and session state currently return to defaults on reboot. Battery
monitoring, shot-location plots and SD logging remain future integration work.

## BLE contract

| Attribute | UUID | Access / initial value |
|---|---|---|
| Service | `8c7a0001-6c3b-4f3d-a8d9-2adbc9f10211` | Primary service |
| TX | `8c7a0002-6c3b-4f3d-a8d9-2adbc9f10211` | Read, notify / `READY` |
| RX | `8c7a0003-6c3b-4f3d-a8d9-2adbc9f10211` | Read, write, write without response / `WRITE PING` |

Subscribe to TX before expecting notifications. The local MTU is 185, but
outgoing records remain at most 20 bytes for default-MTU clients. Each test shot
keeps the app-compatible `{"hit":N,"score":S}` JSON notification, where N is the
shot-count digit modulo 10 (the tenth Classic shot uses 0). The complete count
and total are kept in the session; each score stays in the 0..10 range.

Example final payload and its exact bytes:

```text
Text: {"hit":1,"score":10}
Hex:  7B 22 68 69 74 22 3A 31 2C 22 73 63 6F 72 65 22 3A 31 30 7D
Bits: 01111011 00100010 01101000 01101001 01110100
      00100010 00111010 00110001 00101100 00100010
      01110011 01100011 01101111 01110010 01100101
      00100010 00111010 00110001 00110000 01111101
```

RX accepts these case-sensitive text commands (no newline):

| Command | Effect |
|---|---|
| `PING` | Returns the existing `{"pong":1}` response |
| `RESET` | Clears current session; returns state records |
| `STATE` | Returns mode, shot count/remaining, total/last score, distance and sensitivity |
| `MODE:FREESTYLE` / `MODE:CLASSIC` | Changes mode and resets session when mode actually changes |
| `DIST:10.0M` / `DIST:25.0FT` | Sets distance and display units; validates physical range |
| `CAL:1500` | Sets sensitivity threshold (0..4095) |
| `TEST` / `TEST:7` | Generates a cycling test shot or a specified test score (0..10) |

Rapid and invalid commands return `ERR:COMMAND`; invalid distance returns
`ERR:DIST RANGE`. State responses are separate printable ASCII records such as
`MODE:CLASSIC`, `SHOTS:3 LEFT:7`, `TOTAL:27 LAST:8`, `DIST:10.0 m`, and `CAL:1000`.
The phone's current shot parser remains unchanged. These extra records can be
viewed as diagnostic messages, and do not imply the phone has new settings UI.
The original Bluetooth address allocation and GATT UUIDs are preserved.

## Wiring

| Signal | GPIO |
|---|---:|
| SD CS (held inactive) | 4 |
| Display MISO | 9 |
| Backlight | 10 |
| Display SCLK | 11 |
| Display MOSI | 12 |
| Display D/C | 13 |
| Display reset | 14 |
| Display CS | 3 |
| Touch interrupt | 5 |
| Touch SDA | 6 |
| Touch reset | 7 |
| Touch SCL | 8 |

Display SPI is mode 0, MSB first, 80 MHz on SPI2. The project-owned ST7796S
initialization, RGB565 byte order, palettes, glyphs, and layout are retained.
Touch uses I2C0 at 400 kHz, address 0x38, repeated-start register reads, and
the existing portrait-to-landscape coordinate mapping.

## Source layout

- `main/main.cpp`: UI layout, touch navigation, animation and command dispatch.
- `main/session.cpp`: testable Freestyle/Classic rules and settings values.
- `main/graphics.cpp`: project-owned 4-bit indexed framebuffer, font, primitives
  and ST7796S initialization (76,800 bytes for the framebuffer, no PSRAM needed).
- `main/hardware.cpp`: ESP-IDF GPIO, SPI/DMA, I2C and timing.
- `main/bluetooth.cpp`: ESP-IDF Bluedroid GAP/GATT server.
- `sdkconfig.defaults`: normal target and SDK configuration.

## Validation

`tests/session_test.cpp` covers session rollover, zero and maximum scores,
mode changes, unit conversions, bounds and formatting. On a machine with a host
C++ compiler it can run through `cmake -S tests -B build/session-tests`, followed
by a build and CTest. It also runs on the ESP32 in the optional diagnostic build.

`PrecisionShot diagnostics` in `idf.py menuconfig` enables startup UI tests.
That build exercises the actual touch-dispatch functions, checks state and
renderer invariants, and prints named framebuffer captures over USB. It restores
initial state after the tests. Disable diagnostics for normal operation.
Framebuffer captures prove rendered content, not physical panel appearance or
finger-touch accuracy. Physical acceptance still includes both fullscreen views,
menu animation, settings controls and the DEBUG +HIT button.

See `VALIDATION.md` for the checks performed on the connected device.
# Menu and button artwork

Google Material Icons (filled) are converted ahead of time from the retained SVG
sources in `assets/google-material-icons/` to 24x24, 1-bit arrays in `main/icons.h`.
The project-owned renderer draws foreground pixels transparently using the active
palette. No SVG parser, icon font, Arduino code, or third-party firmware library is
required. Apache 2.0 license, conversion notice, and source URLs accompany the assets.
The approved set covers navigation, mode selection, settings, units, Bluetooth,
theme, reset, and debug shot controls; existing touch areas and actions are retained.
