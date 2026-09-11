# Training UI validation

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
