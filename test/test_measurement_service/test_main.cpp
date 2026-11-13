#include <unity.h>

#include "services/MeasurementService.h"

// ======================================================
// Cảm biến giả: trả kết quả theo cờ ok, đếm số lần đọc
// ======================================================

class FakeSensor : public ISensorProvider {
public:
    bool begin() override { return true; }

    bool read(HealthReading& out) override {
        reads++;
        out.temperatureValid = ok;
        return ok;
    }

    void reportStatus(DeviceStatus&) const override {}

    bool ok = true;
    int reads = 0;
};

void setUp() {}
void tearDown() {}

// Chạy update mỗi 100 ms từ fromMs tới toMs
static void runUntil(MeasurementService& service, uint32_t fromMs, uint32_t toMs) {
    for (uint32_t now = fromMs; now <= toMs; now += 100) {
        service.update(now);
    }
}

void test_healthy_sensor_keeps_its_interval() {
    FakeSensor sensor;
    MeasurementService service;
    service.addSource(sensor, 1000);
    service.begin(0);

    runUntil(service, 100, 10000);

    // Lần đọc trong begin + 10 lần theo chu kỳ 1 giây
    TEST_ASSERT_EQUAL_INT(11, sensor.reads);
    TEST_ASSERT_EQUAL_UINT32(1000, service.currentIntervalMs(0));
}

void test_failing_sensor_backs_off() {
    FakeSensor sensor;
    sensor.ok = false;
    MeasurementService service;
    service.addSource(sensor, 1000);
    service.begin(0);

    runUntil(service, 100, 30000);

    // 3 lần lỗi đầu theo chu kỳ 1 giây (0, 1s, 2s), sau đó mỗi 10 giây (12s, 22s)
    TEST_ASSERT_EQUAL_INT(5, sensor.reads);
    TEST_ASSERT_EQUAL_UINT32(MeasurementService::RETRY_INTERVAL_MS, service.currentIntervalMs(0));
}

void test_recovered_sensor_returns_to_normal_interval() {
    FakeSensor sensor;
    sensor.ok = false;
    MeasurementService service;
    service.addSource(sensor, 1000);
    service.begin(0);

    runUntil(service, 100, 3000);
    TEST_ASSERT_EQUAL_UINT32(MeasurementService::RETRY_INTERVAL_MS, service.currentIntervalMs(0));

    // Cảm biến được nối lại: lần thử kế tiếp thành công thì về chu kỳ cũ
    sensor.ok = true;
    runUntil(service, 3100, 12000);

    TEST_ASSERT_EQUAL_UINT32(1000, service.currentIntervalMs(0));

    int before = sensor.reads;
    runUntil(service, 12100, 15000);
    TEST_ASSERT_EQUAL_INT(before + 3, sensor.reads);
}

void test_slow_source_is_not_shortened_by_backoff() {
    FakeSensor sensor;
    sensor.ok = false;
    MeasurementService service;
    service.addSource(sensor, 20000);
    service.begin(0);

    runUntil(service, 100, 70000);

    TEST_ASSERT_EQUAL_UINT32(20000, service.currentIntervalMs(0));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_healthy_sensor_keeps_its_interval);
    RUN_TEST(test_failing_sensor_backs_off);
    RUN_TEST(test_recovered_sensor_returns_to_normal_interval);
    RUN_TEST(test_slow_source_is_not_shortened_by_backoff);
    return UNITY_END();
}
