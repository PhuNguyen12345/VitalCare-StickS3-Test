#include <time.h>
#include <unity.h>

#include "domain/TimeInfo.h"

void setUp() {}
void tearDown() {}

// gmtime_r là hàm POSIX, Windows (MinGW/UCRT) chỉ có gmtime_s với thứ tự tham số đảo ngược
static void utcFromEpoch(time_t raw, struct tm& out) {
#ifdef _WIN32
    gmtime_s(&out, &raw);
#else
    gmtime_r(&raw, &out);
#endif
}

static void assertTime(uint16_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi, uint8_t s, const TimeInfo& t) {
    TEST_ASSERT_EQUAL_UINT16(y, t.year);
    TEST_ASSERT_EQUAL_UINT8(mo, t.month);
    TEST_ASSERT_EQUAL_UINT8(d, t.day);
    TEST_ASSERT_EQUAL_UINT8(h, t.hour);
    TEST_ASSERT_EQUAL_UINT8(mi, t.minute);
    TEST_ASSERT_EQUAL_UINT8(s, t.second);
}

void test_known_dates() {
    assertTime(1970, 1, 1, 0, 0, 0, fromEpoch(0));
    assertTime(2000, 2, 29, 0, 0, 0, fromEpoch(951782400));    // năm nhuận chia hết 400
    assertTime(2024, 2, 29, 12, 30, 15, fromEpoch(1709209815)); // năm nhuận
    assertTime(2024, 12, 31, 23, 59, 59, fromEpoch(1735689599)); // cuối năm
    assertTime(2100, 3, 1, 0, 0, 0, fromEpoch(4107542400u));   // 2100 không nhuận
}

void test_round_trip_matches_libc() {
    // Mỗi bước ~ 1 ngày 1 giờ 1 phút 1 giây, từ 1970 tới 2105
    for (uint32_t epoch = 0; epoch < 4260000000u; epoch += 90061u) {
        TimeInfo t = fromEpoch(epoch);
        TEST_ASSERT_EQUAL_UINT32(epoch, toEpoch(t));

        time_t raw = (time_t)epoch;
        struct tm expected;
        utcFromEpoch(raw, expected);

        TEST_ASSERT_EQUAL_INT(expected.tm_year + 1900, t.year);
        TEST_ASSERT_EQUAL_INT(expected.tm_mon + 1, t.month);
        TEST_ASSERT_EQUAL_INT(expected.tm_mday, t.day);
        TEST_ASSERT_EQUAL_INT(expected.tm_hour, t.hour);
        TEST_ASSERT_EQUAL_INT(expected.tm_wday, weekdayFromEpoch(epoch));
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_known_dates);
    RUN_TEST(test_round_trip_matches_libc);
    return UNITY_END();
}
