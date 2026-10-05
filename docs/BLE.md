# Bluetooth commands

The device name is **PrecisionShot**. Subscribe to TX, then write ASCII commands
to RX. UUIDs have stayed the same.

| Attribute | UUID | Access |
|---|---|---|
| Service | `8c7a0001-6c3b-4f3d-a8d9-2adbc9f10211` | Primary service |
| TX | `8c7a0002-6c3b-4f3d-a8d9-2adbc9f10211` | Read / notify |
| RX | `8c7a0003-6c3b-4f3d-a8d9-2adbc9f10211` | Read / write |

## Commands

| Command | What it does |
|---|---|
| `STATE` / `PING` | Get state / check the connection |
| `TEST` / `TEST:7` | Cycling test score / chosen score from 0 to 10 |
| `RESET` | Clear the current scores |
| `MODE:FREESTYLE` / `MODE:CLASSIC` | Change mode; reset only when mode changes |
| `NAV:TRAINING` / `NAV:SETTINGS` | Open training / settings |
| `NAV:DEBUGZONE` / `NAV:DEBUG` | Open Debug Zone / Session Debug |
| `FULL:1` / `FULL:0` | Enter / leave fullscreen training |
| `DIST:+` / `DIST:-` | Move distance one step in the chosen unit |
| `DIST:10.0M` / `DIST:25.0FT` | Set distance (physical range 1–100 meters) |
| `UNIT:M` / `UNIT:FT` | Change units without changing the physical distance |
| `CAL:+` / `CAL:-` / `CAL:1500` | Adjust / set sensitivity (0–4095) |
| `THEME:DARK` / `THEME:LIGHT` | Change theme |
| `VIEW:PAYLOAD` / `VIEW:HEX` / `VIEW:BINARY` | Choose packet view |
| `FX:CONFETTI` / `FX:ORBIT` / `FX:BOUNCE` / `FX:WARP` | Play an animation and tune |
| `FX:STOP` | Stop sound and animation |
| `SOUND:TEST` | Legacy command: stop sound; standalone beep removed |
| `WATCH:1` / `WATCH:0` | Start / stop phone Debug Zone sensor readings |
| `MENU:OPEN` / `MENU:CLOSE` | Show / hide the board menu |
| `BLE:RESTART` | Restart advertising if no phone is connected |

Plain commands still work for test tools. The app sends numbered requests such
as `@12:TEST`. Numbers are 1–999. The board replies with state followed by
`ACK:12`, or `ERR:12:COMMAND`, `ERR:12:DIST RANGE`, or `ERR:12:BUSY`. It sends no
success ACK if its state could not be queued. Invalid commands do not change
settings. Do not automatically retry a shot after a timeout; request `STATE`
first to see whether it happened.

## Updates

Each shot still sends `{"hit":N,"score":S}`. The hit number is the shot count
modulo 10. A score is 0–10. There are no sensor coordinates yet.

Protocol version 2 sends complete state updates after commands and touchscreen
changes. Every record is at most 20 bytes, including at the default MTU.
Up to eight records are sent each main-loop pass. The app waits for all fields
and the matching END before displaying the new state:

```text
BEGIN:12
PROTO:2
MODE:CLASSIC
SHOTS:3
LEFT:7
TOTAL:27
LAST:8
DIST:10000
UNIT:M
CAL:1000
PAGE:TRAINING
FULL:0
THEME:DARK
VIEW:PAYLOAD
FX:NONE
SOUND:0
MENU:0
PKTA:{"hit":3,"
PKTB:score":8}
DEL:QUEUED
TLOW:320
THIGH:410
ADC:233
MV:208
DETECT:0
END:12
```

DIST is the actual distance in millimeters. LAST is -1 before the first shot.
SHOTS and TOTAL keep their full 32-bit values. BEGIN/END numbers increase with
each snapshot and restart on boot. Changes to sound include its eventual stop.
On reconnect, old queued messages are cleared and the app requests a fresh state.
PKTA and PKTB join to make the exact stored Session Debug packet. DEL records
whether it was an example, offline, unsubscribed, queued, or failed. Reading this
stored packet does not create another shot event.

TLOW and THIGH are regulator temperatures in tenths of a degree Celsius.
-9999 means unavailable or outside the MCP9700 range. ADC is the averaged
12-bit sensor reading; MV is its calibrated voltage in millivolts. -1 means
unavailable. DETECT is 1 when ADC reaches the sensitivity threshold. Readings
are sampled five times per second and do not create scored shots. The five sensor
fields are only included while the phone watches Debug Zone; other snapshots
have the usual 19 fields. Phone updates are paced at about three per second.
WATCH:1 lasts ten seconds and the visible, active phone renews it every five.
WATCH:0, disconnect, or lease expiry stops readings. Ordinary state and command
confirmations continue when the watch is off. Unwired inputs
can float; an in-range temperature is not proof that a sensor is connected.
