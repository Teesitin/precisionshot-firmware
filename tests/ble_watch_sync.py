"""Quiet checks for sensor subscriptions and command confirmations."""

import asyncio
import json
import time
from pathlib import Path

from bleak import BleakClient, BleakScanner
from ble_acceptance import Board, SERVICE, TX


async def main():
    device = await BleakScanner.find_device_by_filter(
        lambda device, adv: (adv.local_name or device.name) == "PrecisionShot"
        and SERVICE in [uuid.lower() for uuid in adv.service_uuids], timeout=15,
    )
    assert device is not None, "Disconnect the phone before this PC check"
    latencies = []
    captured = []
    async with BleakClient(device, winrt={"use_cached_services": False}) as client:
        board = Board(client)
        await client.start_notify(TX, lambda _, data: board.queue.put_nowait(bytes(data).decode("ascii")))
        await asyncio.sleep(0.2)
        state = await board.command("STATE")
        assert "TLOW" not in state
        baseline = {key: state[key] for key in ["MODE", "SHOTS", "TOTAL", "PAGE", "FULL"]}
        state = await board.command("WATCH:1")
        assert "TLOW" in state and len(state) == 24
        captured.append(state)
        # Repeated read-only requests test ACK delivery under live telemetry.
        for i in range(30):
            started = time.monotonic()
            state = await board.command("WATCH:1" if i % 5 == 0 else "STATE")
            latencies.append(time.monotonic() - started)
            assert all(state[key] == value for key, value in baseline.items())
            assert state["SOUND"] == "0" and state["FX"] == "NONE"
        state = await board.command("WATCH:0")
        assert "TLOW" not in state and len(state) == 19
        captured.append(state)
        # Even later STATE replies must omit readings after the panel closes.
        await asyncio.sleep(0.7)
        assert "TLOW" not in await board.command("STATE")
        await board.command("WATCH:1")
        await asyncio.sleep(10.2)
        assert "TLOW" not in await board.command("STATE"), "Watch lease did not expire"
    # A new connection must not inherit the previous viewer's subscription.
    async with BleakClient(device) as client:
        board = Board(client)
        await client.start_notify(TX, lambda _, data: board.queue.put_nowait(bytes(data).decode("ascii")))
        await asyncio.sleep(0.2)
        assert "TLOW" not in await board.command("STATE")
    Path("build/watch-sync-live.json").write_text(json.dumps(captured, indent=4))
    print(f"PASS 30 confirmations under telemetry; maximum latency {max(latencies):.3f}s")
    print("PASS readings stop on WATCH:0, lease expiry, and reconnect")
    print("PASS scores, mode, page, and fullscreen unchanged; no sound/animation commands")


if __name__ == "__main__":
    asyncio.run(main())
