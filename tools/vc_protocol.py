"""
Giao thức Serial giữa firmware VitalCare (lib/devtools) và công cụ trên máy tính.

Gói ảnh v2 (số nguyên little-endian):
    MAGIC(6) | version u8=2 | flags u8 | width u16 | height u16 | y u16 | rows u16
    | nameLen u8 | name | pixels RGB565 (width*rows*2, byte cao trước) | crc32 u32

Gói ảnh v1 (firmware cũ, luôn là frame đầy đủ):
    MAGIC(6) | width u16 | height u16 | length u32 | nameLen u8 | name | pixels | crc32 u32

Dòng số liệu (text, mỗi giây):
    @DATA rtc=10:00:00 temp=36.52 hr=78 spo2=98 conf=95 status=3 sen=1 page=clock mirror=1
"""

import glob
import struct
import zlib
from dataclasses import dataclass

from PIL import Image

MAGIC = b"\xa5VCIMG"
FLAG_CAPTURE = 0x01

DATA_PREFIX = "@DATA"

_V1_HEADER = struct.Struct("<HHIB")      # width, height, length, nameLen
_V2_HEADER = struct.Struct("<BBHHHHB")   # version, flags, width, height, y, rows, nameLen
_MAX_SIDE = 1024
_MAX_FRAME_BYTES = 1024 * 1024


@dataclass
class Frame:
    version: int
    flags: int
    name: str
    width: int
    height: int
    y: int
    rows: int
    pixels: bytes
    crc_ok: bool

    @property
    def is_capture(self):
        """Ảnh người dùng yêu cầu chụp (s / a / t). Gói v1 luôn là ảnh chụp."""
        return self.version == 1 or bool(self.flags & FLAG_CAPTURE)

    @property
    def is_full(self):
        return self.y == 0 and self.rows == self.height


def find_ports():
    """Các cổng USB CDC có thể là M5StickS3 (macOS / Linux)."""
    return sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))


def rgb565_to_image(pixels, width, height, big_endian=True):
    # Buffer sprite 16-bit của M5GFX lưu byte cao trước (big-endian).
    # Rawmode "BGR;16" của Pillow đọc RGB565 little-endian, nên đảo byte trước.
    if big_endian:
        swapped = bytearray(len(pixels))
        swapped[0::2] = pixels[1::2]
        swapped[1::2] = pixels[0::2]
        pixels = bytes(swapped)

    return Image.frombytes("RGB", (width, height), pixels, "raw", "BGR;16")


class FrameParser:
    """Tách log text và gói ảnh từ luồng byte Serial."""

    def __init__(self, on_text, on_frame):
        self.buffer = bytearray()
        self.on_text = on_text
        self.on_frame = on_frame

    def feed(self, data):
        self.buffer += data

        while True:
            index = self.buffer.find(MAGIC)

            if index < 0:
                # Giữ lại vài byte cuối phòng trường hợp MAGIC bị cắt đôi
                keep = len(MAGIC) - 1
                if len(self.buffer) > keep:
                    self.on_text(bytes(self.buffer[:-keep]))
                    del self.buffer[:-keep]
                return

            if index > 0:
                self.on_text(bytes(self.buffer[:index]))
                del self.buffer[:index]

            result = self._try_parse()

            if result is None:
                return  # Chưa đủ dữ liệu

            if result is False:
                # MAGIC trùng ngẫu nhiên trong log: bỏ 1 byte rồi tìm tiếp
                self.on_text(bytes(self.buffer[:1]))
                del self.buffer[:1]

    def _try_parse(self):
        """None = chờ thêm dữ liệu, False = header không hợp lệ, True = đã xử lý 1 frame."""
        start = len(MAGIC)

        if len(self.buffer) < start + 1:
            return None

        if self.buffer[start] == 2:
            return self._parse_v2(start)

        return self._parse_v1(start)

    def _parse_v2(self, start):
        header_end = start + _V2_HEADER.size

        if len(self.buffer) < header_end:
            return None

        version, flags, width, height, y, rows, name_length = _V2_HEADER.unpack_from(self.buffer, start)
        length = width * rows * 2

        if (
            not 0 < width <= _MAX_SIDE
            or not 0 < height <= _MAX_SIDE
            or rows == 0
            or y + rows > height
            or length > _MAX_FRAME_BYTES
        ):
            return False

        return self._emit(header_end, name_length, length, version, flags, width, height, y, rows)

    def _parse_v1(self, start):
        header_end = start + _V1_HEADER.size

        if len(self.buffer) < header_end:
            return None

        width, height, length, name_length = _V1_HEADER.unpack_from(self.buffer, start)

        if not 0 < width <= _MAX_SIDE or length != width * height * 2 or length > _MAX_FRAME_BYTES:
            return False

        return self._emit(header_end, name_length, length, 1, FLAG_CAPTURE, width, height, 0, height)

    def _emit(self, header_end, name_length, length, version, flags, width, height, y, rows):
        pixel_start = header_end + name_length
        crc_start = pixel_start + length
        total = crc_start + 4

        if len(self.buffer) < total:
            return None

        name = self.buffer[header_end:pixel_start].decode("ascii", "replace")
        pixels = bytes(self.buffer[pixel_start:crc_start])
        (expected_crc,) = struct.unpack_from("<I", self.buffer, crc_start)

        del self.buffer[:total]

        self.on_frame(Frame(
            version=version,
            flags=flags,
            name=name,
            width=width,
            height=height,
            y=y,
            rows=rows,
            pixels=pixels,
            crc_ok=zlib.crc32(pixels) == expected_crc,
        ))
        return True


class FrameBuffer:
    """Ghép các dải dòng nhận được thành ảnh màn hình hoàn chỉnh."""

    def __init__(self, big_endian=True):
        self.big_endian = big_endian
        self.image = None
        self.name = ""

    def apply(self, frame):
        """Trả về True nếu frame đã được dán vào ảnh."""
        if not frame.crc_ok:
            return False

        size = (frame.width, frame.height)

        if self.image is None or self.image.size != size:
            # Chưa có ảnh nền thì chỉ nhận frame đầy đủ
            if not frame.is_full:
                return False
            self.image = Image.new("RGB", size)

        strip = rgb565_to_image(frame.pixels, frame.width, frame.rows, self.big_endian)
        self.image.paste(strip, (0, frame.y))
        self.name = frame.name
        return True


def parse_data_line(line):
    """Tách dòng '@DATA k=v ...' thành dict, trả về None nếu không phải dòng @DATA."""
    line = line.strip()

    if not line.startswith(DATA_PREFIX):
        return None

    values = {}
    for token in line[len(DATA_PREFIX):].split():
        key, separator, value = token.partition("=")
        if separator:
            values[key] = value

    return values
