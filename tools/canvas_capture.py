#!/usr/bin/env python3
"""
Serial monitor + chụp canvas từ M5StickS3.

- In log text từ thiết bị ra terminal như Serial Monitor.
- Nhận gói ảnh canvas (RGB565) và lưu thành PNG trong thư mục captures/.
- Gõ lệnh rồi Enter để gửi xuống thiết bị:
    s = chụp frame hiện tại     a = bật/tắt tự chụp khi chuyển trang
    n = giả lập Button A        c = giả lập Button B (về đồng hồ)
    t = dải màu kiểm tra        q = thoát

Định dạng gói ảnh: xem tools/vc_protocol.py.
Script này chỉ lưu ảnh chụp (s / a / t), bỏ qua luồng mirror liên tục.

Cách dùng:
    python3 tools/canvas_capture.py                 # tự tìm cổng
    python3 tools/canvas_capture.py /dev/cu.usbmodem1101
    python3 tools/canvas_capture.py --swap          # nếu màu bị sai
"""

import argparse
import sys
import threading
from datetime import datetime
from pathlib import Path

import serial
from PIL import Image

from vc_protocol import FrameBuffer, FrameParser, find_ports


def find_port():
    ports = find_ports()
    if not ports:
        sys.exit("Không tìm thấy cổng Serial. Hãy truyền cổng cụ thể làm tham số.")
    return ports[0]


def main():
    parser = argparse.ArgumentParser(description="Serial monitor + chụp canvas M5StickS3")
    parser.add_argument("port", nargs="?", help="Cổng Serial (mặc định: tự tìm)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--out", default="captures", help="Thư mục lưu PNG")
    parser.add_argument("--swap", action="store_true", help="Đảo thứ tự byte RGB565")
    parser.add_argument("--scale", type=int, default=2, help="Phóng to ảnh khi lưu")
    args = parser.parse_args()

    port = args.port or find_port()
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    def on_text(data):
        sys.stdout.write(data.decode("utf-8", "replace"))
        sys.stdout.flush()

    def on_frame(frame):
        if not frame.is_capture:
            return

        if not frame.crc_ok:
            print(f"\n[capture] Bỏ frame '{frame.name}': sai CRC (dữ liệu bị lỗi khi truyền)")
            return

        buffer = FrameBuffer(big_endian=not args.swap)
        if not buffer.apply(frame):
            return

        image = buffer.image
        if args.scale > 1:
            image = image.resize((frame.width * args.scale, frame.height * args.scale), Image.NEAREST)

        stamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]
        path = out_dir / f"{stamp}_{frame.name}.png"
        image.save(path)
        print(f"\n[capture] Đã lưu {path} ({frame.width}x{frame.height})")

    connection = serial.Serial(port, args.baud, timeout=0.05)
    print(f"Đã mở {port}. Lệnh: s, a, n, c, t, q (gõ rồi Enter)")

    stop = threading.Event()

    def read_commands():
        for line in sys.stdin:
            command = line.strip()
            if command == "q":
                break
            if command:
                connection.write(command.encode("ascii", "ignore"))
        stop.set()

    threading.Thread(target=read_commands, daemon=True).start()

    frame_parser = FrameParser(on_text, on_frame)

    try:
        while not stop.is_set():
            data = connection.read(connection.in_waiting or 1)
            if data:
                frame_parser.feed(data)
    except KeyboardInterrupt:
        pass
    finally:
        connection.close()
        print("\nĐã đóng cổng Serial.")


if __name__ == "__main__":
    main()
