#include "domain/MeasurementLog.h"

#include <stdio.h>
#include <stdlib.h>

size_t formatCsvLine(const LogRecord& record, char* buffer, size_t size) {
    int length = snprintf(
        buffer,
        size,
        "%lu,%d,%d,%u,%u,%u,%u\n",
        (unsigned long)record.epoch,
        record.temperatureX100,
        record.bodyTemperatureX100,
        record.heartRate,
        record.spo2,
        record.confidence,
        record.flags
    );

    if (length < 0 || (size_t)length >= size) {
        return 0;
    }

    return (size_t)length;
}

bool parseCsvLine(const char* line, LogRecord& out) {
    if (line == nullptr || *line == '#' || *line == '\0') {
        return false;
    }

    constexpr int COLUMN_COUNT = 7;

    long values[COLUMN_COUNT];
    const char* cursor = line;

    for (int i = 0; i < COLUMN_COUNT; i++) {
        char* end = nullptr;
        values[i] = strtol(cursor, &end, 10);

        if (end == cursor) {
            return false;
        }

        bool last = i == COLUMN_COUNT - 1;

        if (!last && *end != ',') {
            return false;
        }

        if (last && *end != '\0' && *end != '\n' && *end != '\r') {
            return false;
        }

        cursor = end + 1;
    }

    out.epoch = (uint32_t)values[0];
    out.temperatureX100 = (int16_t)values[1];
    out.bodyTemperatureX100 = (int16_t)values[2];
    out.heartRate = (uint16_t)values[3];
    out.spo2 = (uint8_t)values[4];
    out.confidence = (uint8_t)values[5];
    out.flags = (uint8_t)values[6];

    return true;
}
