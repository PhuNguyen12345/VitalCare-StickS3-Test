#!/usr/bin/env python3
"""
Client BLE trên máy tính để kiểm thử firmware VitalCare trước khi có app điện thoại.
Giao thức: docs/BLE_PROTOCOL.md

Cách dùng:
    python3 tools/ble_client.py scan                 # tìm thiết bị
    python3 tools/ble_client.py status               # đọc trạng thái
    python3 tools/ble_client.py live                 # in dữ liệu realtime (Ctrl+C để dừng)
    python3 tools/ble_client.py settime              # chỉnh giờ theo đồng hồ máy tính
    python3 tools/ble_client.py sync                 # đồng bộ log về ble_logs/*.csv
    python3 tools/ble_client.py selftest             # kiểm tra bộ giải mã (không cần thiết bị)

Tuỳ chọn: --name VitalCare-1A2B (chọn đúng thiết bị khi có nhiều máy)
macOS: cần cấp quyền Bluetooth cho Terminal / VS Code (System Settings > Privacy & Security > Bluetooth).
"""

import argparse
import asyncio
import struct
import sys
import zlib
from datetime import datetime, timedelta, timezone
from pathlib import Path

SERVICE_UUID = "06b40001-765e-4d79-a126-f95ef3affd61"
LIVE_UUID = "06b40002-765e-4d79-a126-f95ef3affd61"
CONTROL_UUID = "06b40003-765e-4d79-a126-f95ef3affd61"
SYNC_UUID = "06b40004-765e-4d79-a126-f95ef3affd61"
STATUS_UUID = "06b40005-765e-4d79-a126-f95ef3affd61"

NAME_PREFIX = "VitalCare-"

LIVE_FORMAT = struct.Struct("<IhHBBBBHH")    # 16 byte
STATUS_FORMAT = struct.Struct("<BBBIhHII")   # 19 byte
SYNC_HEADER = struct.Struct("<HHB")          # 5 byte
SEGMENT_END = struct.Struct("<II")           # totalBytes, crc32

TEMP_INVALID = -32768

SYNC_DATA = 0
SYNC_SEGMENT_END = 1
SYNC_DONE = 2

OP_SET_TIME = 0x01
OP_SYNC_START = 0x02
OP_SYNC_ACK = 0x03

LOG_FLAGS = {0x01: "TIME_INVALID", 0x02: "TEMP_INVALID", 0x04: "BIO_INVALID", 0x08: "LOOSE", 0x10: "ALERT"}
STATUS_FLAGS = {0x01: "RTC_VALID", 0x02: "SEN_READY", 0x04: "SYNCING", 0x08: "LOG_OVERFLOW"}


# ======================================================
# Mã hóa / giải mã
# ======================================================

def flag_names(value, names):
    return [name for bit, name in names.items() if value & bit] or ["-"]


def decode_live(data):
    epoch, temp, hr, spo2, conf, finger, flags, seq, _ = LIVE_FORMAT.unpack(bytes(data[:LIVE_FORMAT.size]))
    return {
        "epoch": epoch,
        "temperature": None if temp == TEMP_INVALID else temp / 100,
        "hr": hr,
        "spo2": spo2,
        "confidence": conf,
        "finger": finger,
        "flags": flags,
        "seq": seq,
    }


def decode_status(data):
    version, flags, battery, epoch, tz, segments, pending, free = STATUS_FORMAT.unpack(bytes(data[:STATUS_FORMAT.size]))
    return {
        "version": version,
        "flags": flags,
        "battery": battery,
        "epoch": epoch,
        "tz_minutes": tz,
        "pending_segments": segments,
        "pending_bytes": pending,
        "fs_free_bytes": free,
    }


def encode_set_time(epoch_utc, tz_minutes):
    return struct.pack("<BIh", OP_SET_TIME, epoch_utc, tz_minutes)


def encode_ack(segment_id):
    return struct.pack("<BH", OP_SYNC_ACK, segment_id)


def format_epoch(epoch, tz_minutes=None):
    if epoch == 0:
        return "--"
    moment = datetime.fromtimestamp(epoch, timezone.utc)
    if tz_minutes is not None:
        moment = moment.astimezone(timezone(timedelta(minutes=tz_minutes)))
    else:
        moment = moment.astimezone()
    return moment.strftime("%Y-%m-%d %H:%M:%S")


def print_status(status):
    print(f"  Phiên bản giao thức : {status['version']}")
    print(f"  Cờ                 : {', '.join(flag_names(status['flags'], STATUS_FLAGS))}")
    print(f"  Pin                : {status['battery']}%")
    print(f"  Giờ thiết bị       : {format_epoch(status['epoch'], status['tz_minutes'])} (UTC{status['tz_minutes'] / 60:+g})")
    print(f"  Segment chờ sync   : {status['pending_segments']} ({status['pending_bytes']} byte)")
    print(f"  Bộ nhớ trống       : {status['fs_free_bytes'] // 1024} KB")


def self_test():
    """Giải mã đúng các vector byte trong test/test_ble_protocol (firmware)."""
    live = decode_live(bytes.fromhex("78563412440e4e00625f030402010000"))
    assert live == {"epoch": 0x12345678, "temperature": 36.52, "hr": 78, "spo2": 98,
                    "confidence": 95, "finger": 3, "flags": 0x04, "seq": 0x0102}, live

    assert decode_live(bytes(4) + bytes.fromhex("0080") + bytes(10))["temperature"] is None

    status = decode_status(bytes.fromhex("010557000000 65a401 0300 409c0000 40420f00".replace(" ", "")))
    assert status == {"version": 1, "flags": 0x05, "battery": 87, "epoch": 0x65000000, "tz_minutes": 420,
                      "pending_segments": 3, "pending_bytes": 40000, "fs_free_bytes": 1000000}, status

    assert encode_set_time(0x12345678, -300) == bytes.fromhex("0178563412d4fe")
    assert encode_ack(0x012A) == bytes.fromhex("032a01")

    end = bytes.fromhex("07000300011027000026 39f4cb".replace(" ", ""))
    assert SYNC_HEADER.unpack(end[:5]) == (7, 3, SYNC_SEGMENT_END)
    assert SEGMENT_END.unpack(end[5:]) == (10000, 0xCBF43926)
    assert zlib.crc32(b"123456789") == 0xCBF43926

    print("selftest OK: giải mã khớp với vector của firmware")


# ======================================================
# BLE
# ======================================================

async def find_device(name, timeout):
    from bleak import BleakScanner

    def match(device, advertisement):
        device_name = device.name or advertisement.local_name or ""
        if name:
            return device_name == name
        return SERVICE_UUID in [uuid.lower() for uuid in advertisement.service_uuids] or device_name.startswith(NAME_PREFIX)

    device = await BleakScanner.find_device_by_filter(match, timeout=timeout)
    if device is None:
        sys.exit("Không tìm thấy thiết bị VitalCare. Kiểm tra thiết bị đã bật và không bị điện thoại khác kết nối.")
    return device


async def cmd_scan(args):
    from bleak import BleakScanner

    found = await BleakScanner.discover(timeout=args.timeout, return_adv=True)
    rows = [
        (device, adv) for device, adv in found.values()
        if SERVICE_UUID in [u.lower() for u in adv.service_uuids] or (device.name or adv.local_name or "").startswith(NAME_PREFIX)
    ]

    if not rows:
        print("Không thấy thiết bị VitalCare nào.")
        return

    for device, adv in rows:
        print(f"{device.name or adv.local_name or '?':20}  {device.address}  RSSI {adv.rssi} dBm")


async def connect(args):
    from bleak import BleakClient

    device = await find_device(args.name, args.timeout)
    client = BleakClient(device)
    await client.connect()
    print(f"Đã kết nối {device.name} ({device.address}), MTU {client.mtu_size}")
    return client


async def cmd_status(args):
    client = await connect(args)
    try:
        print_status(decode_status(await client.read_gatt_char(STATUS_UUID)))
    finally:
        await client.disconnect()


async def cmd_settime(args):
    client = await connect(args)
    try:
        now = datetime.now().astimezone()
        epoch = int(now.timestamp())
        tz_minutes = int(now.utcoffset().total_seconds() // 60)

        await client.write_gatt_char(CONTROL_UUID, encode_set_time(epoch, tz_minutes), response=True)
        print(f"Đã gửi SET_TIME: {now:%Y-%m-%d %H:%M:%S} (UTC{tz_minutes / 60:+g})")

        await asyncio.sleep(1.5)
        print_status(decode_status(await client.read_gatt_char(STATUS_UUID)))
    finally:
        await client.disconnect()


async def cmd_live(args):
    client = await connect(args)

    def on_live(_sender, data):
        live = decode_live(data)
        temp = "--.--" if live["temperature"] is None else f"{live['temperature']:.2f}"
        print(
            f"{format_epoch(live['epoch'])}  temp={temp}  hr={live['hr']:3}  spo2={live['spo2']:3}  "
            f"conf={live['confidence']:3}  finger={live['finger']}  "
            f"flags={','.join(flag_names(live['flags'], LOG_FLAGS))}  seq={live['seq']}"
        )

    try:
        await client.start_notify(LIVE_UUID, on_live)
        print("Đang nhận dữ liệu realtime (Ctrl+C để dừng)...")
        if args.seconds > 0:
            await asyncio.sleep(args.seconds)
        else:
            await asyncio.Event().wait()
    finally:
        await client.disconnect()


async def cmd_sync(args):
    client = await connect(args)
    packets = asyncio.Queue()
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    try:
        await client.start_notify(SYNC_UUID, lambda _sender, data: packets.put_nowait(bytes(data)))

        status = decode_status(await client.read_gatt_char(STATUS_UUID))
        print(f"Thiết bị có {status['pending_segments']} segment ({status['pending_bytes']} byte) chờ đồng bộ")

        await client.write_gatt_char(CONTROL_UUID, bytes([OP_SYNC_START]), response=True)

        buffer = bytearray()
        saved = 0

        while True:
            try:
                packet = await asyncio.wait_for(packets.get(), timeout=args.idle_timeout)
            except asyncio.TimeoutError:
                print(f"Không nhận được gói nào trong {args.idle_timeout}s, dừng.")
                break

            segment_id, chunk_index, kind = SYNC_HEADER.unpack(packet[:SYNC_HEADER.size])
            payload = packet[SYNC_HEADER.size:]

            if kind == SYNC_DATA:
                buffer += payload

            elif kind == SYNC_SEGMENT_END:
                total, crc = SEGMENT_END.unpack(payload[:SEGMENT_END.size])

                if total != len(buffer) or crc != zlib.crc32(buffer):
                    print(f"Segment {segment_id}: sai CRC/kích thước ({len(buffer)}/{total} byte), không ACK")
                    buffer.clear()
                    continue

                path = out_dir / f"seg_{segment_id:05d}_{datetime.now():%Y%m%d_%H%M%S}.csv"
                path.write_bytes(buffer)
                records = sum(1 for line in buffer.decode("ascii", "replace").splitlines() if line and not line.startswith("#"))
                print(f"Segment {segment_id}: {len(buffer)} byte, {records} bản ghi, {chunk_index} chunk -> {path}")

                await client.write_gatt_char(CONTROL_UUID, encode_ack(segment_id), response=True)
                saved += 1
                buffer.clear()

            elif kind == SYNC_DONE:
                print(f"Đồng bộ xong: {saved} segment")
                break

        print_status(decode_status(await client.read_gatt_char(STATUS_UUID)))
    finally:
        await client.disconnect()


def main():
    parser = argparse.ArgumentParser(description="Client BLE kiểm thử firmware VitalCare")
    parser.add_argument("command", choices=["scan", "status", "live", "settime", "sync", "selftest"])
    parser.add_argument("--name", help="Tên thiết bị, vd VitalCare-1A2B")
    parser.add_argument("--timeout", type=float, default=8.0, help="Thời gian tìm thiết bị (giây)")
    parser.add_argument("--seconds", type=float, default=0, help="live: số giây nhận dữ liệu (0 = tới khi Ctrl+C)")
    parser.add_argument("--out", default="ble_logs", help="sync: thư mục lưu CSV")
    parser.add_argument("--idle-timeout", type=float, default=15.0, help="sync: dừng nếu không có gói trong N giây")
    args = parser.parse_args()

    if args.command == "selftest":
        self_test()
        return

    commands = {
        "scan": cmd_scan,
        "status": cmd_status,
        "live": cmd_live,
        "settime": cmd_settime,
        "sync": cmd_sync,
    }

    try:
        asyncio.run(commands[args.command](args))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
