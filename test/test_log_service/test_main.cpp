#include <unity.h>

#include <vector>

#include "services/LogService.h"

// Repository trong bộ nhớ, chỉ ghi lại các bản ghi
class FakeRepository : public ILogRepository {
public:
    std::vector<LogRecord> records;

    bool begin() override { return true; }
    bool append(const LogRecord& record) override {
        records.push_back(record);
        return true;
    }
    bool rotate() override { return true; }
    bool oldestSegment(uint16_t&, uint32_t&) override { return false; }
    size_t readSegment(uint16_t, uint32_t, uint8_t*, size_t) override { return 0; }
    bool deleteSegment(uint16_t) override { return false; }
    bool clear() override { return true; }
    LogStats stats() const override { return LogStats(); }
};

static HealthReading reading(float temperature, uint16_t hr, uint16_t spo2, uint8_t confidence, uint8_t status) {
    HealthReading value;
    value.temperatureC = temperature;
    value.temperatureValid = true;
    value.heartRate = hr;
    value.spo2 = spo2;
    value.confidence = confidence;
    value.fingerStatus = status;
    value.bioValid = true;
    return value;
}

// Chạy LogService từ 0 tới endMs, mỗi 100ms một lần update
template <typename ReadingFn>
static void run(LogService& service, uint32_t endMs, ReadingFn readingAt, uint32_t epoch, bool reliable) {
    for (uint32_t now = 100; now <= endMs; now += 100) {
        service.update(now, readingAt(now), epoch == 0 ? 0 : epoch + now / 1000, reliable);
    }
}

void setUp() {}
void tearDown() {}

void test_no_record_before_window_ends() {
    FakeRepository repo;
    LogService service(repo, 1000, 10000);
    service.begin(0);

    run(service, 9900, [](uint32_t) { return reading(36.5f, 80, 98, 90, 3); }, 1761550000, true);

    TEST_ASSERT_EQUAL(0, repo.records.size());
}

void test_averages_valid_samples() {
    FakeRepository repo;
    LogService service(repo, 1000, 10000);
    service.begin(0);

    // Nửa đầu 36.00 / 70 bpm, nửa sau 37.00 / 90 bpm
    run(service, 10000, [](uint32_t now) {
        return now <= 5000 ? reading(36.0f, 70, 96, 80, 3) : reading(37.0f, 90, 98, 100, 3);
    }, 1761550000, true);

    TEST_ASSERT_EQUAL(1, repo.records.size());
    const LogRecord& record = repo.records[0];
    TEST_ASSERT_EQUAL_UINT32(1761550010, record.epoch);
    TEST_ASSERT_EQUAL_INT16(3650, record.temperatureX100);
    TEST_ASSERT_EQUAL_UINT16(80, record.heartRate);
    TEST_ASSERT_EQUAL_UINT8(97, record.spo2);
    TEST_ASSERT_EQUAL_UINT8(90, record.confidence);
    TEST_ASSERT_EQUAL_UINT8(0, record.flags);
}

void test_no_finger_sets_bio_invalid() {
    FakeRepository repo;
    LogService service(repo, 1000, 10000);
    service.begin(0);

    run(service, 10000, [](uint32_t) { return reading(36.5f, 0, 0, 0, 1); }, 1761550000, true);

    const LogRecord& record = repo.records[0];
    TEST_ASSERT_EQUAL_UINT8(LogFlags::BIO_INVALID, record.flags);
    TEST_ASSERT_EQUAL_UINT16(0, record.heartRate);
    TEST_ASSERT_EQUAL_INT16(3650, record.temperatureX100);
}

void test_invalid_temperature_and_time() {
    FakeRepository repo;
    LogService service(repo, 1000, 10000);
    service.begin(0);

    run(service, 10000, [](uint32_t) {
        HealthReading value = reading(0, 75, 97, 90, 3);
        value.temperatureValid = false;
        return value;
    }, 0, false);

    const LogRecord& record = repo.records[0];
    TEST_ASSERT_EQUAL_UINT8(LogFlags::TEMP_INVALID | LogFlags::TIME_INVALID, record.flags);
    TEST_ASSERT_EQUAL_UINT32(10, record.epoch);  // Thời gian chạy (giây)
    TEST_ASSERT_EQUAL_UINT16(75, record.heartRate);
}

void test_one_record_per_window() {
    FakeRepository repo;
    LogService service(repo, 1000, 10000);
    service.begin(0);

    run(service, 60000, [](uint32_t) { return reading(36.5f, 80, 98, 90, 3); }, 1761550000, true);

    TEST_ASSERT_EQUAL(6, repo.records.size());
    TEST_ASSERT_EQUAL(6, service.recordsWritten());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_no_record_before_window_ends);
    RUN_TEST(test_averages_valid_samples);
    RUN_TEST(test_no_finger_sets_bio_invalid);
    RUN_TEST(test_invalid_temperature_and_time);
    RUN_TEST(test_one_record_per_window);
    return UNITY_END();
}
