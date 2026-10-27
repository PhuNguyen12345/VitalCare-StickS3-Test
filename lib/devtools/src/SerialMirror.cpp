#include "SerialMirror.h"

#if VC_ENABLE_DEVTOOLS

namespace {

constexpr uint8_t FRAME_MAGIC[] = {0xA5, 'V', 'C', 'I', 'M', 'G'};
constexpr uint8_t FRAME_VERSION = 2;

uint32_t crcTable[256];
bool crcTableReady = false;

void buildCrcTable() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;

        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }

        crcTable[i] = crc;
    }

    crcTableReady = true;
}

// CRC32 chuẩn (giống zlib.crc32 bên Python)
uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t length) {
    crc = ~crc;

    for (size_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crcTable[(crc ^ data[i]) & 0xFF];
    }

    return ~crc;
}

template <typename T>
void appendLE(uint8_t*& cursor, T value) {
    for (size_t i = 0; i < sizeof(T); i++) {
        *cursor++ = (uint8_t)(value >> (8 * i));
    }
}

}  // namespace

SerialMirror::SerialMirror(Print& out) : out_(out) {
    if (!crcTableReady) {
        buildCrcTable();
    }
}

void SerialMirror::setMirrorEnabled(bool enabled) {
    mirrorEnabled_ = enabled;

    // Bật lại mirror thì frame kế tiếp gửi đầy đủ
    cacheValid_ = false;
}

bool SerialMirror::toggleAutoCapture() {
    autoCapture_ = !autoCapture_;
    return autoCapture_;
}

void SerialMirror::onFrame(const uint8_t* pixels, uint16_t width, uint16_t height, const char* name) {
    bool pageChanged = strncmp(name, lastName_, MAX_NAME_LENGTH) != 0;

    strncpy(lastName_, name, MAX_NAME_LENGTH);
    lastName_[MAX_NAME_LENGTH] = '\0';

    bool capture = captureRequested_ || (autoCapture_ && pageChanged);

    if (!capture && !mirrorEnabled_) {
        return;
    }

    uint16_t rows = min(height, MAX_ROWS);
    size_t rowBytes = (size_t)width * 2;

    int32_t firstChanged = -1;
    int32_t lastChanged = -1;

    for (uint16_t y = 0; y < rows; y++) {
        uint32_t crc = crc32Update(0, pixels + y * rowBytes, rowBytes);

        if (!cacheValid_ || crc != rowCrc_[y]) {
            if (firstChanged < 0) {
                firstChanged = y;
            }

            lastChanged = y;
        }

        rowCrc_[y] = crc;
    }

    cacheValid_ = true;

    bool sent = true;

    if (capture) {
        captureRequested_ = false;
        sent = sendRows(pixels, width, rows, 0, rows, name, FLAG_CAPTURE);
    } else if (firstChanged >= 0) {
        sent = sendRows(
            pixels,
            width,
            rows,
            firstChanged,
            lastChanged - firstChanged + 1,
            name,
            0
        );
    }

    // Gửi lỗi thì frame kế tiếp gửi lại đầy đủ
    if (!sent) {
        cacheValid_ = false;
    }
}

bool SerialMirror::sendRows(
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t y,
    uint16_t rows,
    const char* name,
    uint8_t flags
) {
    const uint8_t* region = pixels + (size_t)y * width * 2;
    uint32_t length = (uint32_t)width * rows * 2;
    uint8_t nameLength = (uint8_t)min(strlen(name), (size_t)MAX_NAME_LENGTH);

    uint8_t header[sizeof(FRAME_MAGIC) + 11 + MAX_NAME_LENGTH];
    uint8_t* cursor = header;

    memcpy(cursor, FRAME_MAGIC, sizeof(FRAME_MAGIC));
    cursor += sizeof(FRAME_MAGIC);

    appendLE(cursor, FRAME_VERSION);
    appendLE(cursor, flags);
    appendLE(cursor, width);
    appendLE(cursor, height);
    appendLE(cursor, y);
    appendLE(cursor, rows);
    appendLE(cursor, nameLength);

    memcpy(cursor, name, nameLength);
    cursor += nameLength;

    uint8_t footer[4];
    uint8_t* footerCursor = footer;
    appendLE(footerCursor, crc32Update(0, region, length));

    uint32_t deadline = millis() + WRITE_TIMEOUT_MS;

    out_.flush();

    bool ok =
        writeAll(header, cursor - header, deadline) &&
        writeAll(region, length, deadline) &&
        writeAll(footer, sizeof(footer), deadline);

    out_.flush();

    if (!ok) {
        out_.println("\nCapture failed: serial write timeout");
    }

    return ok;
}

// Ghi đủ số byte, chia nhỏ để không tràn bộ đệm USB CDC
bool SerialMirror::writeAll(const uint8_t* data, size_t length, uint32_t deadline) {
    size_t sent = 0;

    while (sent < length) {
        if ((int32_t)(millis() - deadline) > 0) {
            return false;
        }

        size_t chunk = min((size_t)512, length - sent);
        size_t written = out_.write(data + sent, chunk);

        if (written == 0) {
            delay(1);
        }

        sent += written;
    }

    return true;
}

#endif
