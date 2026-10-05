# Training UI validation

## Main Board V4 pin correction (2026-10-02)

- Root cause of the white display: the firmware used the development-board
  jumper pin map while the display was mounted on Main Board V4. Corrected all
  12 LCD, touch and SD-select GPIO assignments in `main/hardware.cpp`.
- Independently checked each firmware GPIO against the supplied
  `C:/Users/teesitin/Downloads/Main Board V4.brd` net/contact references and the
  WROOM pad-to-GPIO mapping in the matching September 17 schematic. All 12
  assignments match, with no duplicate GPIOs. README wiring now describes the PCB.
- Native ESP-IDF 6.0.2 build passed with project warnings as errors; no Arduino
  or additional firmware dependencies. LCD SPI remains at the conservative
  20 MHz bring-up setting. Application size: 682,496 bytes.
- Flashed bootloader, partition table and application to COM10 on ESP32-S3
  base MAC `94:A9:90:D2:F7:F0`; esptool verified all three written hashes.
  No whole-flash erase was performed.
- Captured a fresh boot: Main Board V4 pin banner, successful FT6336 chip ID
  read `0x64`, BLE advertising as PrecisionShot, and the normal `[READY]` banner.
  Chip ID logging now checks the I2C result instead of printing failed-read data.
- User confirmed the physical screen displays the PrecisionShot interface.
  Physical touch navigation and BLE interaction were not retested in this pass.
- Evidence (generated, ignored): `build/pcb-v4-flash.log` and
  `build/pcb-v4-boot.log`.

## Debug Zone animation update (2026-09-11)

- Rapid removed from the navigation menu. Settings now includes Debug Zone.
  Four nonblocking five-second `HIGHSCORE XXX` demos: Confetti Chaos, Cosmic
  Orbit, Jelly Bounce, and Warp Speed. Playback returns to the picker; close
  cancels playback and the picker's close returns to Settings.
- ESP-IDF diagnostic build flashed and tested through the board's CH343 UART
  connection on COM7 (same ESP32-S3 base MAC `94:A9:90:D2:3D:8C`). Temporary
  diagnostic console: UART0 GPIO43/44 at 921600 baud, to collect frame dumps.
- Device tests passed all four real-time five-second runs (return checked within
  200 ms of the deadline), early cancellation, navigation and preservation of
  session shot counts/totals. Existing session, settings, renderer and synthetic
  touch tests also passed. This is not a physical finger test.
- Sixteen complete frame dumps decoded. Visually inspected the four effects,
  picker, Settings entry and menu without Rapid. Captured renders plus display
  transfer took 44-48 ms. BLE processing stays in the main loop; radio traffic
  during animation was not separately exercised.
- Evidence: `build/animations-ui-serial.log`, `build/animation-frames/`, and
  `build/animations-final-flash.log`. Normal build restores USB Serial/JTAG
  console and disables diagnostic tests.
- Normal firmware flashed on COM7 with verified hashes. Boot confirmed by
  reading BLE `READY` from the original device identity. Application 682,208
  bytes; SHA-256 `fc867f607ac9c6d8b6f81fd818143304a21a1c58c3afecbe1d173fc1347d7e1e`.
  Final boot evidence: `build/animations-final-boot.log`.

## Google bitmap icon update (2026-09-11)

- Integrated the approved Google Material filled artwork as 24x24 monochrome
  arrays. All 20 arrays match the approved binary assets byte-for-byte.
- Native ESP-IDF 6.0.2 production build passes with project warnings as errors.
  No new firmware or host dependencies were installed.
- Normal icon firmware flashed on COM6 with esptool hash verification, then
  rebooted to `[READY]` with diagnostics disabled and touch chip ID `0x64`.
  Application: 676,016 bytes; SHA-256
  `b86afcceb64be6f7ab036b999d63810848c8bd9782db2617b97e0e21e7553503`.
  Boot evidence: `build/icons-final-boot.log`.
- Device diagnostic tests on COM6 pass session/settings, synthetic touch actions,
  transparent bitmap pixels, edge clipping, and the existing renderer checks.
- All 11 framebuffer captures decoded completely and were visually inspected,
  including dark/light settings, menu animation, fullscreen and debug views.
  Icon placement and labels fit their controls; render plus transfer takes 44-48 ms.
- Evidence: `build/icons-ui-serial.log`, `build/icons-frames/`,
  `build/icons-build.log`, and `build/icons-final-flash.log` (generated, ignored).
- These are framebuffer and synthetic touch checks, not a physical finger test.
  The BLE protocol and touch hitboxes were unchanged; the earlier BLE results below
  were not rerun for this artwork-only update.

The following records describe the earlier training UI baseline.

Date: 2026-09-11. Native ESP-IDF 6.0.2; ESP32-S3 on COM6.

## Firmware left on the device

- Freestyle and Classic UI, fullscreen views, settings and session debugger.
- Rapid is visibly disabled and cannot be selected by touch or BLE.
- CPU 240 MHz; existing wiring, display setup and original BLE address
  `94:A9:90:D2:3D:8D` preserved. No third-party firmware dependencies or Arduino.
- Normal build has `CONFIG_PRECISIONSHOT_SELF_TEST` disabled. The diagnostic
  session test source is excluded from the normal build.
- Application size: 674,384 bytes, within the 1 MB app partition (36% free).
- Application SHA-256:
  `586158a464d19dc65ba94b2a51e731c3ef58d9bc1410fe056ce7696a0f25e453`.
- Build succeeds with `-Wall -Wextra -Werror` on project sources. All nonempty
  component paths are within the project or the official installed SDK.
- Esptool verified flash hashes. Final reset reaches the native training UI
  `[READY]` banner and reads FT6336 chip ID `0x64`, without running self-tests.

## Session and UI checks executed on the ESP32

The optional diagnostic build invokes the actual UI touch-dispatch functions;
these are synthetic touch inputs, not an automated physical finger test.

- Classic starts with 10 shots left, shows the last score during the round,
  shows the total after shot 10, and counts shot 11 as shot 1 of a new round.
- Total 100, total zero, misses, score bounds, resets and mode changes pass.
- Freestyle continues beyond 25 shots without completing a Classic round.
- Normal/fullscreen buttons, fullscreen close, bottom-right DEBUG +HIT,
  session reset, menu navigation, disabled Rapid and Debug back navigation pass.
- Distance +/-, feet/meters conversion, physical distance preservation,
  sensitivity +/-, bounds, and theme toggle pass.
- Renderer checks cover packed pixels, clipping, font glyphs and palette.
- Drawer animation advances through an intermediate position and reaches
  both open and closed endpoints.

Eleven actual framebuffer captures were decoded and visually inspected:
Classic normal, Classic fullscreen, Classic total 100, Freestyle fullscreen,
menu intermediate/open, settings dark/light, debug payload/hex/binary.
The captured layouts fit the 480x320 screen. Rendering plus panel transfer for
these frames took **45-48 ms**. This is a screen-render measurement, not a
measurement of physical laser detection-to-display latency.

## Bluetooth checks executed against the normal build

- Original advertisement/name/service/MAC, characteristic properties, initial
  values and MTU 185.
- PING using writes with and without response, plus characteristic readback.
- Classic ten perfect shots: correct <=20-byte JSON notifications, total 100,
  zero remaining; next score 7 starts a new round with nine remaining.
- Freestyle twenty-five shots remain an ongoing session.
- Distance in meters/feet, range boundaries, malformed values, NaN and overflow
  rejected without corrupting the prior setting.
- Sensitivity 0..4095, boundaries and invalid commands.
- Rapid and out-of-range test scores rejected.
- RESET retains settings and restarts the debug score sequence 10/9/8/7.
- Unsubscribe suppresses notifications. Disconnect, rediscovery, cached service
  reconnection, resubscribe, PING and further test shots succeed.
- Harness checked 346 notifications and restored an empty Classic session,
  distance 10.0 m and sensitivity 1000. A final reset left the normal fresh UI.

## Evidence and remaining physical checks

Generated evidence (ignored by Git):

- `build/ui-diagnostic-build.log`, `build/ui-diagnostic-flash.log`
- `build/ui-serial.log`, `build/frames/*.png`
- `build/training-final-build.log`, `build/training-final-flash.log`
- `build/training-ble-tests.log`, `build/training-final-boot.log`

The PC capture/decoder/BLE scripts are diagnostic tools, not firmware libraries.
Physical finger accuracy, apparent animation smoothness and readability at the
trainer's actual distance still require observation on the panel. The user had
confirmed the prior native display/touch firmware worked before these changes.

Sensor scanning and baseline calibration are not connected yet. The sensitivity
control stores configuration only; distance and sensitivity metadata in Session
Debug are the values at the time of the last simulated shot. The ordinary shot
notification remains the existing app-compatible score packet. No actual shot
location, battery telemetry or calibration success is fabricated. Settings and
session state currently reset to defaults on reboot.

## Speaker debug test — October 5, 2026

- Main Board V4.brd: SPK connects U1 pad 36 (GPIO44) to LM386 pin 3; both SPK1/SPK2 share the amplifier output through C10.
- Added Settings -> Debug Zone speaker test: three 4 kHz, 150 ms beeps with 150 ms gaps, nonblocking timing and repeat-tap guard.
- ESP-IDF 6.0.2 esp32s3 build passed; image size 0xa8a50, 34% app partition free. Git whitespace check passed.
- No flashing or hardware playback performed, per user request. Physical touch target and audible output from each speaker remain to be checked.

## Public-domain melody files — October 5, 2026

- Retained four Mutopia LilyPond scores; each edition declares Public Domain.
- Converted monophonic excerpts into separate native C++ frequency/duration
  tables: Entertainer 4000 ms, Mountain King 6957 ms, Fur Elise 3438 ms,
  Greensleeves 6000 ms. Sources, attribution, and conversion choices are in
  main/music/README.md and generate_excerpts.py.
- Generalized the existing nonblocking speaker sequencer to support pitched
  notes, rests, cancellation, and busy rejection. The original three-beep
  sequence remains three 150 ms 4 kHz tones separated by 150 ms rests.
- ESP-IDF 6.0.2 esp32s3 build passed; binary 0xa8cb0, 34% partition free.
  Verified generated files match the transcription data, all event durations
  are positive, pitches are in supported bounds, and total durations match.
- Animation buttons are not wired to melody playback yet. No flashing or
  physical sound checks performed. Build log: build/music-build.log.

## Debug Zone animation music — October 5, 2026

- Connected Confetti to The Entertainer, Orbit to Greensleeves, Bounce to Fur
  Elise, and Warp to Mountain King; the picker displays each tune's name.
- Preserved the user's latest single 150 ms, 2 kHz speaker-test beep and label.
- Orbit runs six seconds and Warp 6957 ms so their full excerpts finish;
  Confetti and Bounce retain five seconds. Updated the remaining-time bar.
  Starting an animation replaces an active beep, and close/page exit/end
  silences the sequencer.
- Updated optional embedded UI checks for longer durations, active music and
  cancellation. These device checks were not run; no flashing was performed.
- Normal ESP-IDF 6.0.2 esp32s3 build passed, binary 0xa8fc0, 34% partition free.
  Whitespace checks passed. Log: build/animation-music-build.log. Physical
  animation, tune playback, and perceived beep pitch remain to be checked.

## Code cleanup — October 5, 2026

- Used four-space indentation in our C++ code, headers, and test files. Added
  .clang-format and kept generated icon bytes in compact rows.
- Added short section comments and simplified existing comments. Shortened the
  main README with a project overview, current features, GPIO count/table,
  build commands, and file guide. Moved detailed Bluetooth notes to docs/BLE.md.
  Music credits and original score files are retained.
- Compared all 18 C++ source/header/include files against the working code
  before cleanup: code tokens are identical after removing comments/spacing.
  No functional bug fixes or behavior changes were needed for this pass.
- Formatter checks passed. The Python generator parses and matches all four
  formatted tune files. Project code has no tabs, README links exist, and Git
  whitespace checks passed.
- Final ESP-IDF 6.0.2 esp32s3 build passed: binary 0xa8fc0, 34% partition free.
  Log: build/cleanup-final-build.log. Nothing was flashed. Physical checks and
  optional startup device tests were not rerun during this cleanup.

## Connected app and production screens — October 5, 2026

- Training keeps Settings, Reset, and DEBUG +HIT. Session Debug, packet views,
  animation tunes, and the beep test are inside Settings → Debug Zone.
- Added numbered commands and complete version-2 state snapshots for the app.
  The board sends touchscreen changes too. Notifications are queued and paced;
  BEGIN/END protect partial updates, and ACK identifies a confirmed command.
- The on-board BLE acceptance run passed 71 confirmed commands and 1517
  notifications: Classic completion/rollover, Freestyle, reset, settings,
  navigation, fullscreen, packet tabs, all four tunes, stop, beep, rejection,
  and reconnect. An earlier run was interrupted by the user touching the board;
  the untouched repeat passed. Log: build/connected-ble-tests.log.
- Added the stored debug packet and its delivery status to snapshots. A final
  read-only STATE request passed against the production board, and the app's
  parser accepted its exact packet and empty session. This check played no sound.
- Startup diagnostics captured the revised screens but hit the old animation
  timing assertion after Orbit. The allowance now includes the final frame and
  picker redraw. The repeat was stopped at the user's request because of the
  loud beeper; the full startup UI test was not completed.
- Restored and flashed the normal build on COM10. Startup tests are disabled
  in generated configuration and absent from the ELF symbols. Esptool verified
  written data. Binary: 0xa9b20 (695072 bytes), 34% app partition free.
  Logs: build/connected-build.log and build/connected-production-flash.log.
- Four-space formatter and whitespace checks passed. No further sound or
  animation tests were run after the user requested they stop.
# Regulator and sensor input update — 2026-10-05

- Checked regulator and Main Board V4 netlists: MCP9700 outputs use GPIO9/10;
  PSB_ANALOG uses GPIO1, J_PSB pin 9.
- ESP-IDF 6.0.2 build passed; normal startup tests remain disabled.
- Flashed COM10 successfully, with esptool verifying written hashes.
- Read three complete 24-field BLE snapshots using STATE only. Temperatures
  were 31.9–32.6 C and 41.3–41.4 C; sensor input was 136–208 mV, below threshold.
- The phone's real parser accepted all captured snapshots. No sound or
  animation tests were run. Temperature accuracy and applied-voltage threshold
  crossing still need physical checks with a reference meter/source.
# Debug Zone layout and refresh update — 2026-10-05

- Moved readings into temperature and sensor panels above compact animation
  buttons. Updated touch bounds to match, with Session Debug and Stop Sound below.
- Removed the standalone beep button. Refresh and sampling have no sound calls.
- Sampling runs every 200 ms; automatic BLE state publishing is paced at 300 ms
  to leave room for commands. Forced command responses still publish immediately.
- ESP-IDF build and COM10 flash passed, with written hashes verified.
- Three live BLE snapshots passed quietly: SOUND stayed 0 and FX stayed NONE.
  Detection reflected raw ADC values above and below the threshold.
- No board UI or sound tests were run, following the user's earlier request.
# Idle speaker investigation — 2026-10-05

- User reports beep noise on each screen update despite SOUND:0.
- Removed the standalone test-beep routine, including legacy SOUND:TEST playback.
  Deliberately selected music remains available.
- Idle PWM is stopped and disconnected; GPIO44 is held low at boot and stop.
- Build and COM10 flash passed, with written hashes verified.
- Acoustic silence is awaiting the user's check after the final flash. Noise
  coupling from display activity through the amplifier or supply is possible.
- User confirmed noise persists after the final flash and occurs on every
  screen update. Software changes did not resolve the audible symptom.
  Main Board V4 C6 is 10 uF across LM386 pins 1/8, selecting gain 200;
  reduced gain and amplifier supply/ground isolation need a physical check.
# Phone sensor watch and command sync — 2026-10-05

- Sensor fields are omitted unless the phone requests WATCH:1. WATCH:0,
  disconnect, and ten-second lease expiry stop sensor traffic. Core state remains.
- Notifications drain up to eight records per loop; read-only STATE/WATCH
  commands no longer force a display redraw.
- Build and COM10 flash passed, with hashes verified.
- Quiet hardware test passed 30 matched state/ACK confirmations under active
  telemetry; maximum observed latency was 0.391 s. Stop, expiry, and reconnect
  checks passed. No scores, modes, pages, fullscreen, sound, or animations changed.
- No touchscreen action or audio tests were run. Shared state serialization
  still publishes local firmware actions and remote command results.
