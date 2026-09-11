# Firmware rules

- Use native ESP-IDF and standard C/C++ only. Arduino is prohibited, including
  Arduino as an ESP-IDF component or a compatibility layer.
- Do not add third-party firmware libraries, managed components, or vendored
  drivers. Use Espressif's SDK APIs and project-owned display/touch code.
- Preserve the display wiring, landscape orientation, UI behavior, and BLE
  protocol unless a task explicitly changes them.
- Build with ESP-IDF 6.0.2 for `esp32s3`. Keep generated build output out of Git.
- A successful build does not prove physical display/touch or BLE behavior;
  record what was actually checked on hardware and any remaining checks.
