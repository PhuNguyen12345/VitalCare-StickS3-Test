#include <string.h>
#include <unity.h>

#include "domain/MeasurementLog.h"

void setUp() {}
void tearDown() {}

void test_format_line() {
    LogRecord record;
    record.epoch = 1761550000;
    record.temperatureX100 = 3652;
    record.heartRate = 78;
    record.spo2 = 98;
    record.confidence = 95;
    record.flags = 0;

    char line[48];
    size_t length = formatCsvLine(record, line, sizeof(line));

    TEST_ASSERT_EQUAL_STRING("1761550000,3652,78,98,95,0\n", line);
    TEST_ASSERT_EQUAL(strlen(line), length);
}

void test_format_rejects_small_buffer() {
    LogRecord record;
    char line[8];
    TEST_ASSERT_EQUAL(0, formatCsvLine(record, line, sizeof(line)));
}

void test_round_trip_negative_temperature_and_flags() {
    LogRecord record;
    record.epoch = 123;
    record.temperatureX100 = -1250;
    record.flags = LogFlags::BIO_INVALID | LogFlags::TIME_INVALID;

    char line[48];
    formatCsvLine(record, line, sizeof(line));

    LogRecord parsed;
    TEST_ASSERT_TRUE(parseCsvLine(line, parsed));
    TEST_ASSERT_EQUAL_UINT32(123, parsed.epoch);
    TEST_ASSERT_EQUAL_INT16(-1250, parsed.temperatureX100);
    TEST_ASSERT_EQUAL_UINT8(LogFlags::BIO_INVALID | LogFlags::TIME_INVALID, parsed.flags);
}

void test_parse_rejects_header_and_garbage() {
    LogRecord parsed;
    TEST_ASSERT_FALSE(parseCsvLine(LOG_CSV_HEADER, parsed));
    TEST_ASSERT_FALSE(parseCsvLine("1,2,3", parsed));
    TEST_ASSERT_FALSE(parseCsvLine("1,2,3,4,5,x", parsed));
    TEST_ASSERT_FALSE(parseCsvLine("", parsed));
    TEST_ASSERT_TRUE(parseCsvLine("1,2,3,4,5,6\r\n", parsed));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_format_line);
    RUN_TEST(test_format_rejects_small_buffer);
    RUN_TEST(test_round_trip_negative_temperature_and_flags);
    RUN_TEST(test_parse_rejects_header_and_garbage);
    return UNITY_END();
}
