#!/usr/bin/env python3
"""
Simulator M5StickS3: mirror màn hình thời gian thực + nút A/B ảo.

Thiết bị là nguồn sự thật duy nhất: nút ảo gửi lệnh xuống firmware,
firmware xử lý y như nút thật rồi gửi frame mới lên. App không tự vẽ UI.

- Click nút ảo  -> n / c      (giống click BtnA / BtnB)
- Giữ >= 800 ms -> N / C      (giống giữ BtnA / BtnB)
- Phím tắt: a / b (giữ phím để hold)

Cách dùng:
    python3 tools/vc_simulator.py                   # chọn cổng trong app
    python3 tools/vc_simulator.py /dev/cu.usbmodem1101
    python3 tools/vc_simulator.py --swap            # nếu màu bị sai
"""

import argparse
import queue
import threading
import time
import tkinter as tk
from datetime import datetime
from pathlib import Path
from tkinter import ttk

import serial
from PIL import Image, ImageTk

from vc_protocol import FrameBuffer, FrameParser, find_ports, parse_data_line

# Khớp với AppConfig::BUTTON_HOLD_MS trong firmware
HOLD_MS = 800

SCREEN_WIDTH = 240
SCREEN_HEIGHT = 135
SCALE = 2

LOG_MAX_LINES = 2000
POLL_MS = 20

# Chờ một chút trước khi coi là nhả phím, để lọc cặp nhả/nhấn của auto-repeat
KEY_RELEASE_DEBOUNCE_MS = 40

BODY_COLOR = "#e8662a"
BODY_EDGE = "#b84a17"
BUTTON_COLOR = "#f2f2f2"
BUTTON_ACTIVE = "#9be38a"
BUTTON_HOLD = "#f5c542"
ERROR_COLOR = "#d93025"
MUTED_TEXT = "#9a9a9a"


class SerialWorker(threading.Thread):
    """Đọc Serial ở thread riêng, đẩy sự kiện vào queue cho UI."""

    def __init__(self, port, baud, events):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.events = events
        self.connection = None
        self.stopping = threading.Event()
        self.write_lock = threading.Lock()

    def run(self):
        try:
            self.connection = serial.Serial(self.port, self.baud, timeout=0.05)
        except serial.SerialException as error:
            self.events.put(("error", f"Không mở được {self.port}: {error}"))
            return

        self.events.put(("connected", self.port))

        parser = FrameParser(
            on_text=lambda data: self.events.put(("text", data)),
            on_frame=lambda frame: self.events.put(("frame", frame)),
        )

        try:
            while not self.stopping.is_set():
                data = self.connection.read(self.connection.in_waiting or 1)
                if data:
                    parser.feed(data)
        except (serial.SerialException, OSError) as error:
            if not self.stopping.is_set():
                self.events.put(("error", f"Mất kết nối Serial: {error}"))
        finally:
            self.connection.close()
            self.events.put(("disconnected", self.port))

    def send(self, command):
        if self.connection is None or not self.connection.is_open:
            return

        with self.write_lock:
            try:
                self.connection.write(command.encode("ascii"))
            except (serial.SerialException, OSError):
                pass

    def stop(self):
        self.stopping.set()


class VirtualButton:
    """Mô phỏng nút thật: nhả trước HOLD_MS = click, giữ đủ HOLD_MS = hold."""

    def __init__(self, root, click_command, hold_command, send, on_visual):
        self.root = root
        self.click_command = click_command
        self.hold_command = hold_command
        self.send = send
        self.on_visual = on_visual

        self.pressed = False
        self.held = False
        self.timer = None

    def press(self, _event=None):
        # Bỏ qua auto-repeat của bàn phím khi đang giữ
        if self.pressed:
            return

        self.pressed = True
        self.held = False
        self.timer = self.root.after(HOLD_MS, self._on_hold)
        self.on_visual("pressed")

    def release(self, _event=None):
        if not self.pressed:
            return

        self.pressed = False

        if self.timer is not None:
            self.root.after_cancel(self.timer)
            self.timer = None

        if not self.held:
            self.send(self.click_command)

        self.on_visual("idle")

    def _on_hold(self):
        self.timer = None

        if self.pressed:
            self.held = True
            self.send(self.hold_command)
            self.on_visual("hold")


class SimulatorApp:
    DATA_FIELDS = [
        ("rtc", "RTC"),
        ("temp", "Nhiệt độ (°C)"),
        ("hr", "Nhịp tim (BPM)"),
        ("spo2", "SpO2 (%)"),
        ("conf", "Confidence (%)"),
        ("status", "Finger status"),
        ("page", "Trang"),
        ("sen", "SEN"),
        ("bat", "Pin (%)"),
        ("ble", "BLE"),
        ("sync", "Đang đồng bộ"),
        ("logseg", "Segment chờ đồng bộ"),
        ("mirror", "Mirror"),
    ]

    def __init__(self, root, args):
        self.root = root
        self.args = args
        self.events = queue.Queue()
        self.worker = None

        self.frame_buffer = FrameBuffer(big_endian=not args.swap)
        self.screen_dirty = False
        self.photo = None
        self.text_buffer = ""
        self.pending_key_release = {}

        self.stats_started = time.monotonic()
        self.stats_frames = 0
        self.stats_bytes = 0
        self.crc_errors = 0

        root.title("VitalCare StickS3 Simulator")
        root.protocol("WM_DELETE_WINDOW", self.on_close)

        self._build_device()
        self._build_shortcuts()
        self._build_side_panel()
        self._build_status_bar()
        self._bind_keys()

        self.refresh_ports(preferred=args.port)
        self.root.after(POLL_MS, self.process_events)
        self.root.after(1000, self.update_stats)

        if args.port:
            self.connect()

    # ==================================================
    # Giao diện
    # ==================================================

    def _build_device(self):
        margin = 28
        screen_w = SCREEN_WIDTH * SCALE
        screen_h = SCREEN_HEIGHT * SCALE

        body_w = screen_w + margin * 2 + 90
        body_h = screen_h + margin * 2

        self.device = tk.Canvas(
            self.root,
            width=body_w + 20,
            height=body_h + 50,
            highlightthickness=0,
        )
        self.device.grid(row=0, column=0, padx=12, pady=12, sticky="n")

        # Thân máy
        self.device.create_rectangle(
            10, 40, 10 + body_w, 40 + body_h,
            fill=BODY_COLOR, outline=BODY_EDGE, width=3,
        )

        # Viền và màn hình
        screen_x = 10 + margin
        screen_y = 40 + margin
        self.device.create_rectangle(
            screen_x - 6, screen_y - 6, screen_x + screen_w + 6, screen_y + screen_h + 6,
            fill="#111111", outline="",
        )
        self.screen_item = self.device.create_image(screen_x, screen_y, anchor="nw")
        self.placeholder_item = self.device.create_text(
            screen_x + screen_w / 2, screen_y + screen_h / 2,
            text="Chưa có frame\n(Connect để bắt đầu mirror)",
            fill="#888888", justify="center",
        )

        # Nút A: mặt trước, cạnh màn hình
        a_x = screen_x + screen_w + 45
        a_y = screen_y + screen_h / 2
        self.button_a_item = self.device.create_oval(
            a_x - 28, a_y - 28, a_x + 28, a_y + 28,
            fill=BUTTON_COLOR, outline="#555555", width=2, tags="btnA",
        )
        self.device.create_text(a_x, a_y, text="A", font=("Helvetica", 18, "bold"), tags="btnA")

        # Nút B: cạnh trên (bên hông máy)
        b_x = 10 + body_w - 120
        self.button_b_item = self.device.create_rectangle(
            b_x - 40, 22, b_x + 40, 40,
            fill=BUTTON_COLOR, outline="#555555", width=2, tags="btnB",
        )
        self.device.create_text(b_x, 31, text="B", font=("Helvetica", 11, "bold"), tags="btnB")

        self.button_a = VirtualButton(
            self.root, "n", "N", self.send_command,
            lambda state: self._paint_button(self.button_a_item, state),
        )
        self.button_b = VirtualButton(
            self.root, "c", "C", self.send_command,
            lambda state: self._paint_button(self.button_b_item, state),
        )

        for tag, button in (("btnA", self.button_a), ("btnB", self.button_b)):
            self.device.tag_bind(tag, "<ButtonPress-1>", button.press)
            self.device.tag_bind(tag, "<ButtonRelease-1>", button.release)

    # Thao tác, cách dùng chuột / bàn phím, lệnh gửi xuống thiết bị, tác dụng
    SHORTCUTS = [
        ("Click A", "Click nút A", "a", "n", "Sang trang đo kế tiếp"),
        ("Giữ A", f"Giữ nút A ≥ {HOLD_MS} ms", "Giữ a", "N", "Chưa gán chức năng"),
        ("Click B", "Click nút B", "b", "c", "Về trang đồng hồ"),
        ("Giữ B", f"Giữ nút B ≥ {HOLD_MS} ms", "Giữ b", "C", "Chưa gán chức năng"),
        ("Test pattern", "Nút Test pattern", "—", "t", "Hiện dải màu kiểm tra"),
        ("Capture PNG", "Nút Capture PNG", "—", "—", "Lưu ảnh vào captures/"),
    ]

    def _build_shortcuts(self):
        panel = ttk.LabelFrame(self.root, text="Nút ảo & phím tắt")
        panel.grid(row=1, column=0, padx=12, pady=(0, 12), sticky="nsew")

        # Lưới Label thay cho Treeview để tô màu riêng từng cột
        headings = ("Thao tác", "Chuột", "Bàn phím", "Gửi qua serial", "Tác dụng")
        serial_column = 3

        for column, heading in enumerate(headings):
            ttk.Label(
                panel,
                text=heading,
                font="TkHeadingFont",
                foreground=MUTED_TEXT if column == serial_column else "",
            ).grid(row=0, column=column, sticky="w", padx=6, pady=(4, 2))

        ttk.Separator(panel).grid(row=1, column=0, columnspan=len(headings), sticky="ew", padx=6)

        for row, values in enumerate(self.SHORTCUTS, start=2):
            for column, value in enumerate(values):
                ttk.Label(
                    panel,
                    text=value,
                    foreground=MUTED_TEXT if column == serial_column else "",
                ).grid(row=row, column=column, sticky="w", padx=6, pady=1)

        panel.columnconfigure(len(headings) - 1, weight=1)

    def _build_side_panel(self):
        side = ttk.Frame(self.root)
        side.grid(row=0, column=1, rowspan=2, padx=(0, 12), pady=12, sticky="nsew")
        self.root.columnconfigure(1, weight=1)
        self.root.rowconfigure(1, weight=1)

        # Kết nối
        connection = ttk.LabelFrame(side, text="Kết nối")
        connection.pack(fill="x")

        self.port_var = tk.StringVar()
        # readonly: không cho gõ vào ô, tránh phím a/b bị gõ thành tên cổng
        self.port_box = ttk.Combobox(connection, textvariable=self.port_var, width=26, state="readonly")
        self.port_box.grid(row=0, column=0, padx=6, pady=6)

        ttk.Button(connection, text="Làm mới", command=self.refresh_ports).grid(row=0, column=1, padx=2)

        self.connect_button = ttk.Button(connection, text="Connect", command=self.toggle_connection)
        self.connect_button.grid(row=0, column=2, padx=6)

        # Số liệu
        data = ttk.LabelFrame(side, text="Số liệu (@DATA)")
        data.pack(fill="x", pady=(10, 0))

        self.data_labels = {}
        for row, (key, title) in enumerate(self.DATA_FIELDS):
            ttk.Label(data, text=title).grid(row=row, column=0, sticky="w", padx=8, pady=1)
            value = tk.Label(data, text="—", font=("Menlo", 12), anchor="w")
            value.grid(row=row, column=1, sticky="w", padx=8)
            self.data_labels[key] = value

        self.default_fg = value.cget("fg")

        # Công cụ
        tools = ttk.Frame(side)
        tools.pack(fill="x", pady=(10, 0))
        ttk.Button(tools, text="Capture PNG", command=self.capture_png).pack(side="left")
        ttk.Button(tools, text="Test pattern", command=lambda: self.send_command("t")).pack(side="left", padx=6)

        # Log
        log_frame = ttk.LabelFrame(side, text="Log Serial")
        log_frame.pack(fill="both", expand=True, pady=(10, 0))

        self.log = tk.Text(log_frame, height=14, width=60, font=("Menlo", 11), state="disabled", wrap="none")
        scroll = ttk.Scrollbar(log_frame, command=self.log.yview)
        self.log.configure(yscrollcommand=scroll.set)
        self.log.pack(side="left", fill="both", expand=True)
        scroll.pack(side="right", fill="y")

    def _build_status_bar(self):
        self.status_var = tk.StringVar(value="Chưa kết nối")
        ttk.Label(self.root, textvariable=self.status_var, anchor="w").grid(
            row=2, column=0, columnspan=2, sticky="ew", padx=12, pady=(0, 8)
        )

    def _bind_keys(self):
        # bind_all: bắt phím dù widget nào đang giữ focus
        self.root.bind_all("<KeyPress>", self._on_key_press)
        self.root.bind_all("<KeyRelease>", self._on_key_release)

        # Click vào thân máy thì lấy lại focus bàn phím cho app
        self.device.bind("<ButtonPress-1>", lambda _event: self.device.focus_set(), add="+")

        # App mở từ Terminal trên macOS thường chưa có focus bàn phím
        self.root.after(300, self._take_focus)

    def _take_focus(self):
        self.root.lift()
        self.root.focus_force()
        self.device.focus_set()

    def _key_button(self, event):
        # lower(): vẫn nhận khi bật Caps Lock hoặc giữ Shift
        return {"a": self.button_a, "b": self.button_b}.get(event.keysym.lower())

    def _on_key_press(self, event):
        button = self._key_button(event)
        if button is None:
            return

        # Auto-repeat có thể gửi cặp nhả/nhấn liên tục khi giữ phím:
        # nhấn lại ngay sau khi nhả nghĩa là phím vẫn đang được giữ
        pending = self.pending_key_release.pop(button, None)
        if pending is not None:
            self.root.after_cancel(pending)
            return

        button.press()

    def _on_key_release(self, event):
        button = self._key_button(event)
        if button is None:
            return

        self.pending_key_release[button] = self.root.after(
            KEY_RELEASE_DEBOUNCE_MS, lambda: self._release_key(button)
        )

    def _release_key(self, button):
        self.pending_key_release.pop(button, None)
        button.release()

    def _paint_button(self, item, state):
        color = {"pressed": BUTTON_ACTIVE, "hold": BUTTON_HOLD}.get(state, BUTTON_COLOR)
        self.device.itemconfigure(item, fill=color)

    # ==================================================
    # Kết nối
    # ==================================================

    def refresh_ports(self, preferred=None):
        ports = find_ports()
        if preferred and preferred not in ports:
            ports.insert(0, preferred)

        self.port_box["values"] = ports
        if preferred:
            self.port_var.set(preferred)
        elif ports and not self.port_var.get():
            self.port_var.set(ports[0])

    def toggle_connection(self):
        if self.worker is None:
            self.connect()
        else:
            self.disconnect()

    def connect(self):
        port = self.port_var.get().strip()
        if not port:
            self.append_log("[sim] Chưa chọn cổng Serial")
            return

        self.worker = SerialWorker(port, self.args.baud, self.events)
        self.worker.start()
        self.connect_button.configure(text="Disconnect")
        self.status_var.set(f"Đang mở {port}...")

        # Trả focus về thân máy để dùng phím tắt ngay
        self.device.focus_set()

    def disconnect(self):
        if self.worker is None:
            return

        self.send_command("m")
        self.worker.stop()
        self.worker = None
        self.connect_button.configure(text="Connect")

    def send_command(self, command):
        if self.worker is None:
            self.append_log(f"[sim] Chưa kết nối, bỏ lệnh '{command}'. Hãy chọn cổng rồi bấm Connect")
            return

        self.worker.send(command)

    def on_close(self):
        self.disconnect()
        self.root.after(150, self.root.destroy)

    # ==================================================
    # Xử lý dữ liệu từ thiết bị
    # ==================================================

    def process_events(self):
        try:
            while True:
                kind, payload = self.events.get_nowait()

                if kind == "frame":
                    self.on_frame(payload)
                elif kind == "text":
                    self.on_text(payload)
                elif kind == "connected":
                    self.append_log(f"[sim] Đã mở {payload}, bật mirror")
                    self.send_command("M")
                elif kind == "disconnected":
                    self.append_log(f"[sim] Đã đóng {payload}")
                elif kind == "error":
                    self.append_log(f"[sim] {payload}")
                    self.worker = None
                    self.connect_button.configure(text="Connect")
        except queue.Empty:
            pass

        if self.screen_dirty:
            self.screen_dirty = False
            self.redraw_screen()

        self.root.after(POLL_MS, self.process_events)

    def on_frame(self, frame):
        if not frame.crc_ok:
            self.crc_errors += 1
            return

        if self.frame_buffer.apply(frame):
            self.stats_frames += 1
            self.stats_bytes += len(frame.pixels)
            self.screen_dirty = True

    def on_text(self, data):
        self.text_buffer += data.decode("utf-8", "replace")

        *lines, self.text_buffer = self.text_buffer.split("\n")

        for line in lines:
            line = line.rstrip("\r")
            values = parse_data_line(line)

            if values is None:
                self.append_log(line)
            else:
                self.update_data_panel(values)

    def update_data_panel(self, values):
        for key, label in self.data_labels.items():
            value = values.get(key, "—")
            is_error = value == "err" or (key in ("sen", "mirror") and value == "0")
            label.configure(text=value, fg=ERROR_COLOR if is_error else self.default_fg)

        # Thiết bị khởi động lại sẽ tắt mirror: bật lại
        if self.worker is not None and values.get("mirror") == "0":
            self.send_command("M")

    def redraw_screen(self):
        image = self.frame_buffer.image
        if image is None:
            return

        scaled = image.resize((SCREEN_WIDTH * SCALE, SCREEN_HEIGHT * SCALE), Image.NEAREST)
        self.photo = ImageTk.PhotoImage(scaled)
        self.device.itemconfigure(self.screen_item, image=self.photo)
        self.device.itemconfigure(self.placeholder_item, state="hidden")

    def append_log(self, line):
        self.log.configure(state="normal")
        self.log.insert("end", line + "\n")

        line_count = int(self.log.index("end-1c").split(".")[0])
        if line_count > LOG_MAX_LINES:
            self.log.delete("1.0", f"{line_count - LOG_MAX_LINES}.0")

        self.log.see("end")
        self.log.configure(state="disabled")

    def update_stats(self):
        elapsed = max(time.monotonic() - self.stats_started, 1e-6)
        fps = self.stats_frames / elapsed
        kbps = self.stats_bytes / 1024 / elapsed

        if self.worker is not None:
            self.status_var.set(
                f"{self.worker.port}  |  {fps:.1f} frame/s  |  {kbps:.1f} KB/s  |  "
                f"CRC lỗi: {self.crc_errors}  |  trang: {self.frame_buffer.name or '—'}"
            )
        else:
            self.status_var.set("Chưa kết nối")

        self.stats_started = time.monotonic()
        self.stats_frames = 0
        self.stats_bytes = 0
        self.root.after(1000, self.update_stats)

    def capture_png(self):
        image = self.frame_buffer.image
        if image is None:
            self.append_log("[sim] Chưa có frame để chụp")
            return

        out_dir = Path(self.args.out)
        out_dir.mkdir(parents=True, exist_ok=True)

        stamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]
        path = out_dir / f"{stamp}_{self.frame_buffer.name or 'screen'}.png"

        image.resize((SCREEN_WIDTH * SCALE, SCREEN_HEIGHT * SCALE), Image.NEAREST).save(path)
        self.append_log(f"[sim] Đã lưu {path}")


def main():
    parser = argparse.ArgumentParser(description="Simulator M5StickS3 (mirror + nút ảo)")
    parser.add_argument("port", nargs="?", help="Cổng Serial (mặc định: chọn trong app)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--out", default="captures", help="Thư mục lưu PNG")
    parser.add_argument("--swap", action="store_true", help="Đảo thứ tự byte RGB565")
    args = parser.parse_args()

    root = tk.Tk()
    SimulatorApp(root, args)
    root.mainloop()


if __name__ == "__main__":
    main()
