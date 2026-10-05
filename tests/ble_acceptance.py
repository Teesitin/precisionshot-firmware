"""Check the connected board with Bleak on the PC. This does not flash it."""

import asyncio
import json

from bleak import BleakClient, BleakScanner

SERVICE = "8c7a0001-6c3b-4f3d-a8d9-2adbc9f10211"
TX = "8c7a0002-6c3b-4f3d-a8d9-2adbc9f10211"
RX = "8c7a0003-6c3b-4f3d-a8d9-2adbc9f10211"


class Board:
    def __init__(self, client):
        self.client = client
        self.queue = asyncio.Queue()
        self.number = 0
        self.state = None
        self.fields = {}
        self.revision = None
        self.shots = []
        self.records = 0

    async def read(self):
        text = await asyncio.wait_for(self.queue.get(), 6)
        assert 0 < len(text) <= 20, text
        self.records += 1
        if text.startswith("BEGIN:"):
            self.revision = text[6:]
            self.fields = {}
        elif text.startswith("END:"):
            assert self.revision == text[4:], text
            assert len(self.fields) in (19, 24), self.fields
            assert self.fields["PROTO"] == "2"
            self.state = dict(self.fields)
            self.revision = None
        elif text.startswith('{"hit":'):
            self.shots.append(json.loads(text))
        elif self.revision is not None and ":" in text:
            key, value = text.split(":", 1)
            self.fields[key] = value
        return text

    async def command(self, command, **expected):
        self.number = self.number % 999 + 1
        request = f"@{self.number}:{command}"
        assert len(request) <= 20
        await self.client.write_gatt_char(RX, request.encode("ascii"), response=True)
        while True:
            text = await self.read()
            if text == f"ACK:{self.number}":
                break
            if text.startswith(f"ERR:{self.number}:"):
                raise AssertionError(text)
        for key, value in expected.items():
            assert self.state[key] == str(value), (command, key, value, self.state)
        return self.state

    async def rejected(self, command, reason):
        before = await self.command("STATE")
        self.number += 1
        await self.client.write_gatt_char(RX, f"@{self.number}:{command}".encode(), response=True)
        while await self.read() != f"ERR:{self.number}:{reason}":
            pass
        assert self.state == before, (command, before, self.state)

    async def wait_state(self, **expected):
        for _ in range(100):
            await self.read()
            if self.state and all(self.state[key] == str(value) for key, value in expected.items()):
                return
        raise AssertionError(expected)


async def main():
    device = await BleakScanner.find_device_by_filter(
        lambda device, adv: (adv.local_name or device.name) == "PrecisionShot"
        and SERVICE in [uuid.lower() for uuid in adv.service_uuids], timeout=15,
    )
    assert device is not None, "PrecisionShot not found; disconnect the phone first"
    print(f"Board {device.address}", flush=True)
    async with BleakClient(device, winrt={"use_cached_services": False}) as client:
        board = Board(client)
        await client.start_notify(TX, lambda _, data: board.queue.put_nowait(bytes(data).decode("ascii")))
        await board.command("MODE:CLASSIC", MODE="CLASSIC")
        await board.command("RESET", SHOTS=0, LEFT=10, TOTAL=0, LAST=-1)
        total = 0
        for i in range(1, 11):
            score = 10 - ((i - 1) % 4)
            total += score
            await board.command("TEST", SHOTS=i, LEFT=10 - i, TOTAL=total, LAST=score)
            assert board.shots[-1] == {"hit": i % 10, "score": score}
        await board.command("TEST:6", SHOTS=1, LEFT=9, TOTAL=6, LAST=6)
        print("PASS Classic totals, original shot packets, and next-round reset", flush=True)
        await board.command("MODE:FREESTYLE", SHOTS=0, LEFT=0)
        for i in range(1, 14):
            await board.command("TEST:8", SHOTS=i, TOTAL=i * 8)
        await board.command("MODE:FREESTYLE", SHOTS=13)
        print("PASS Freestyle and same-mode score preservation", flush=True)
        await board.command("NAV:SETTINGS", PAGE="SETTINGS", FULL=0)
        await board.command("DIST:10.0M", DIST=10000, UNIT="M")
        await board.command("UNIT:FT", DIST=10000, UNIT="FT")
        await board.command("DIST:+", DIST=10305)
        await board.command("DIST:-", DIST=10000)
        await board.command("UNIT:M", DIST=10000)
        await board.command("CAL:1000", CAL=1000)
        await board.command("CAL:+", CAL=1100)
        await board.command("CAL:-", CAL=1000)
        await board.command("THEME:LIGHT", THEME="LIGHT")
        await board.command("THEME:DARK", THEME="DARK")
        await board.command("FULL:1", PAGE="TRAINING", FULL=1)
        await board.command("FULL:0", FULL=0)
        await board.command("NAV:DEBUGZONE", PAGE="DEBUGZONE")
        await board.command("NAV:DEBUG", PAGE="DEBUG")
        for view in ["HEX", "BINARY", "PAYLOAD"]:
            await board.command(f"VIEW:{view}", VIEW=view)
        await board.command("MENU:OPEN", MENU=1)
        await board.command("MENU:CLOSE", MENU=0)
        print("PASS settings, fullscreen, navigation, packet tabs, and board menu", flush=True)
        for command, reason in [("MODE:RAPID", "COMMAND"), ("DIST:0M", "DIST RANGE"), ("TEST:11", "COMMAND"), ("CAL:4096", "COMMAND")]:
            await board.rejected(command, reason)
        print("PASS invalid commands leave state unchanged", flush=True)
        for effect, seconds in [("CONFETTI", 5), ("ORBIT", 6), ("BOUNCE", 5), ("WARP", 7)]:
            await board.command(f"FX:{effect}", PAGE="DEBUGZONE", FX=effect, SOUND=1, SHOTS=13)
            await asyncio.sleep(seconds + 0.6)
            await board.command("STATE", FX="NONE", SOUND=0, SHOTS=13)
        await board.command("FX:ORBIT", SOUND=1)
        await board.command("FX:STOP", FX="NONE", SOUND=0)
        await board.command("SOUND:TEST")
        await asyncio.sleep(0.3)
        await board.command("STATE", SOUND=0)
        print("PASS four tunes, natural finish, early stop, and test beep", flush=True)
        await board.command("MODE:CLASSIC")
        await board.command("RESET", SHOTS=0, LAST=-1)
        await board.command("NAV:TRAINING", PAGE="TRAINING")
        print(f"PASS {board.number} confirmed requests / {board.records} notifications", flush=True)
    async with BleakClient(device) as client:
        board = Board(client)
        await client.start_notify(TX, lambda _, data: board.queue.put_nowait(bytes(data).decode("ascii")))
        await board.command("STATE", MODE="CLASSIC", SHOTS=0, PAGE="TRAINING", FULL=0)
        print("PASS reconnect and complete fresh state", flush=True)


if __name__ == "__main__":
    asyncio.run(main())
