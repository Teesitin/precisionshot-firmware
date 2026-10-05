# PrecisionShot firmware

PrecisionShot is our laser target training project. This repository runs the
ESP32-S3 main board, touchscreen, Bluetooth connection, and two piezo speakers.
We use C++ with **ESP-IDF 6.0.2**, without Arduino or extra firmware libraries.

## What works now

- **Freestyle:** keep shooting and show the latest score.
- **Classic:** ten shots per round, with a final total out of 100.
- **Settings:** distance, meters/feet, sensitivity, light/dark theme, and Bluetooth.
- **Debug Zone (inside Settings):** Session Debug, packet views, speaker test,
  and four animations with public-domain tunes.

The phone app can control the same scores, settings, screens, and tests. Changes
made on the touchscreen are also sent to the phone. Rapid is not available yet.

`DEBUG +HIT` creates test scores. The real sensor scanning is still to be added.
Settings and scores reset when the board restarts. SD logging and battery
readings are also planned.

## Board and pins

Main Board V4 uses an **ESP32-S3-WROOM-1-N8R8** and a 4-inch **ST7796S** display
with **FT6336** touch. The screen runs at **480 x 320** in landscape mode.

The firmware uses **13 GPIOs** for the display, touch, speaker, and inactive SD
chip select. Native USB uses two more. These are ESP32 GPIO numbers, not module
pad numbers.

| Connection | GPIO |
|---|---:|
| Display MOSI | 11 |
| Display clock | 12 |
| Display MISO | 13 |
| Display chip select | 14 |
| Display data/command | 15 |
| Display reset | 16 |
| Display backlight | 17 |
| SD chip select (held inactive) | 18 |
| Touch SDA | 39 |
| Touch SCL | 40 |
| Touch interrupt | 41 |
| Touch reset | 42 |
| Speaker signal | 44 |
| USB D- / D+ | 19 / 20 |

The display and LM386 amplifier use the 5 V supply; the ESP32 uses 3.3 V.
Both speakers share the LM386 output, so they play the same sound. The speaker
signal is GPIO44, which is pad 36 on the module. The test button plays one
150 ms beep at 2 kHz.

## Build and flash

Open an ESP-IDF 6.0.2 terminal in this folder. On our Windows computer, load it
with:

```powershell
. C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1
```

Then build, flash, or view the serial log:

```powershell
idf.py build
idf.py -p COM10 flash
idf.py -p COM10 monitor
```

Use the board's current COM port if it changes. `build` does not flash anything.
Exit the serial monitor with **Ctrl+]**. The project uses an 8 MB flash layout.

## Files

| File | What it does |
|---|---|
| `main/main.cpp` | Screens, touch buttons, and phone commands |
| `main/remote.inc` | App commands and complete state updates |
| `main/celebration.inc` | Debug animations and tune choices |
| `main/hardware.cpp` | GPIO, speaker timing, display SPI, and touch I2C |
| `main/graphics.cpp` | Drawing, text, icons, and screen updates |
| `main/bluetooth.cpp` | Bluetooth connection and messages |
| `main/session.cpp` | Score rules and settings |
| `main/sensors.cpp` | Read regulator temperatures and the sensor voltage |
| `main/music/` | Four tune files, original scores, and credits |
| `tests/` | Session tests and optional startup UI checks |

The phone sees the device as **PrecisionShot**. It reads updates from TX and
sends commands to RX. See [Bluetooth commands](docs/BLE.md) for the packet
format and UUIDs, and [music notes](main/music/README.md) for the tune sources.

We use four-space indentation; `.clang-format` keeps the C++ style consistent.
Debug Zone shows both MCP9700 regulator temperatures and the sensor ADC input.
Bluetooth sends these readings only while the phone's Debug Zone is visible.
J_REG33 pin 4 goes to GPIO9; J_REG5 pin 4 goes to GPIO10.
For a sensor voltage test, use J_PSB pin 9 (GPIO1) and a shared ground.
Keep this input between 0 and 3.3 V; never use 5 V. ADC readings saturate near
3.1 V. An unconnected input can float, so its reading is not a connection check.
Detection uses the raw ADC sensitivity threshold and does not add scored shots.
Build results and hardware checks are recorded in [VALIDATION.md](VALIDATION.md).
