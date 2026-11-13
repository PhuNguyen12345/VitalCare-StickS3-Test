#include <unity.h>

#include "services/BodyTemperatureEstimator.h"

using namespace TemperatureThresholds;

// ======================================================
// Fake
// ======================================================

class FakeConfig : public IConfigRepository {
public:
    bool hasOffset = false;
    int16_t offsetX100 = 0;

    bool begin() override { return true; }
    int16_t tzMinutes(int16_t defaultValue) override { return defaultValue; }
    void setTzMinutes(int16_t) override {}

    bool tempOffsetX100(int16_t& out) override {
        out = offsetX100;
        return hasOffset;
    }

    void setTempOffsetX100(int16_t value) override {
        hasOffset = true;
        offsetX100 = value;
    }

    void clearTempOffset() override { hasOffset = false; }
};

// ======================================================
// Tiện ích
// ======================================================

static HealthReading skin(float celsius) {
    HealthReading reading;
    reading.temperatureValid = true;
    reading.temperatureC = celsius;
    return reading;
}

// Cấp mẫu mỗi 1 giây từ startMs tới endMs; nhiệt độ theo hàm của thời gian (ms)
template <typename SkinFn>
static void feed(BodyTemperatureEstimator& estimator, uint32_t startMs, uint32_t endMs, SkinFn skinAt) {
    for (uint32_t now = startMs; now <= endMs; now += 1000) {
        estimator.update(now, skin(skinAt(now)));
    }
}

static const uint32_t MINUTE = 60000;

void setUp() {}
void tearDown() {}

// ======================================================
// Test
// ======================================================

void test_starts_unavailable_with_default_offset() {
    BodyTemperatureEstimator estimator(nullptr);
    estimator.begin(0);

    TEST_ASSERT_EQUAL(BodyTemperatureState::Unavailable, estimator.current().state);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, DEFAULT_OFFSET_C, estimator.current().offsetC);
    TEST_ASSERT_FALSE(estimator.current().calibrated);
}

void test_cold_skin_is_not_worn() {
    BodyTemperatureEstimator estimator(nullptr);
    estimator.begin(0);

    feed(estimator, 0, 5 * MINUTE, [](uint32_t) { return 26.0f; });

    TEST_ASSERT_EQUAL(BodyTemperatureState::NotWorn, estimator.current().state);
    TEST_ASSERT_TRUE(isnan(estimator.current().bodyC));
}

void test_sensor_error_is_unavailable() {
    BodyTemperatureEstimator estimator(nullptr);
    estimator.begin(0);
    feed(estimator, 0, 5 * MINUTE, [](uint32_t) { return 34.17f; });

    HealthReading error;
    estimator.update(5 * MINUTE + 10000, error);

    TEST_ASSERT_EQUAL(BodyTemperatureState::Unavailable, estimator.current().state);
}

void test_estimate_after_one_minute_stable_after_three() {
    BodyTemperatureEstimator estimator(nullptr);
    estimator.begin(0);

    // 30 giây đầu: chưa có số
    feed(estimator, 0, 30000, [](uint32_t) { return 34.17f; });
    TEST_ASSERT_EQUAL(BodyTemperatureState::Measuring, estimator.current().state);
    TEST_ASSERT_FALSE(estimator.current().hasEstimate());

    // Sau 1 phút: có số tạm thời, da không đổi nên không bù
    feed(estimator, 31000, MINUTE, [](uint32_t) { return 34.17f; });
    TEST_ASSERT_EQUAL(BodyTemperatureState::Stabilizing, estimator.current().state);
    TEST_ASSERT_TRUE(estimator.current().hasEstimate());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 34.17f + DEFAULT_OFFSET_C, estimator.current().bodyC);

    // Đủ 3 phút ổn định: Stable
    feed(estimator, MINUTE + 1000, 4 * MINUTE, [](uint32_t) { return 34.17f; });
    TEST_ASSERT_EQUAL(BodyTemperatureState::Stable, estimator.current().state);
}

// Đường cong đo ngày 2026-11-03: tăng nhanh sau khi đeo, đứng yên từ khoảng phút 17
static float wearCurve(uint32_t ms) {
    float minutes = ms / 60000.0f;
    return 34.17f - 3.5f * expf(-minutes / 4.0f);
}

void test_rising_skin_is_not_stable_but_predicted() {
    BodyTemperatureEstimator estimator(nullptr);
    estimator.begin(0);

    feed(estimator, 0, 10 * MINUTE, wearCurve);  // Phút 10 vẫn tăng khoảng 0.07 °C/phút

    TEST_ASSERT_EQUAL(BodyTemperatureState::Stabilizing, estimator.current().state);

    // Số tạm thời đã được bù về gần giá trị cân bằng
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 34.17f + DEFAULT_OFFSET_C, estimator.current().bodyC);
}

// ======================================================
// Dữ liệu thật: nhiệt độ da (x100) mỗi 10 giây từ lúc đeo, ble_logs seg 9 và 11.
// Giá trị cân bằng = trung bình phút 15-20 của chính lần đeo đó.
// ======================================================

static const int16_t WEAR_2026_11_03[] = {
    3189, 3196, 3207, 3215, 3223, 3231, 3238, 3245, 3251, 3256,
    3258, 3258, 3264, 3271, 3275, 3275, 3281, 3286, 3286
};
static const float PLATEAU_2026_11_03 = 34.17f;

static const int16_t WEAR_2026_11_04[] = {
    3167, 3172, 3179, 3187, 3190, 3196, 3202, 3208, 3214, 3219,
    3223, 3228, 3234, 3241, 3246, 3249, 3254, 3254, 3254
};
static const float PLATEAU_2026_11_04 = 34.10f;

static void assertRealWear(const int16_t* series, size_t length, float plateau) {
    BodyTemperatureEstimator estimator(nullptr);
    estimator.begin(0);

    uint32_t endMs = (length - 1) * 10000;

    for (uint32_t now = 0; now <= endMs; now += 1000) {
        estimator.update(now, skin(series[now / 10000] / 100.0f));

        // Từ phút 1 trở đi: luôn có số, sai số so với lúc cân bằng không quá 0.7 °C
        if (now >= MINUTE && now % 10000 == 0) {
            TEST_ASSERT_TRUE(estimator.current().hasEstimate());
            TEST_ASSERT_FLOAT_WITHIN(0.7f, plateau + DEFAULT_OFFSET_C, estimator.current().bodyC);
        }
    }
}

void test_real_wear_first_minutes_are_close_to_equilibrium() {
    assertRealWear(WEAR_2026_11_03, sizeof(WEAR_2026_11_03) / sizeof(WEAR_2026_11_03[0]), PLATEAU_2026_11_03);
    assertRealWear(WEAR_2026_11_04, sizeof(WEAR_2026_11_04) / sizeof(WEAR_2026_11_04[0]), PLATEAU_2026_11_04);
}

void test_cooling_skin_is_not_corrected() {
    BodyTemperatureEstimator estimator(nullptr);
    estimator.begin(0);

    // Da nguội dần 0.3 °C/phút (ví dụ ra ngoài trời lạnh): không bù xuống
    feed(estimator, 0, 2 * MINUTE, [](uint32_t ms) { return 34.0f - 0.3f * ms / 60000.0f; });

    const BodyTemperature& body = estimator.current();
    TEST_ASSERT_EQUAL(BodyTemperatureState::Stabilizing, body.state);

    // Trung bình 1 phút gần nhất: phút 1-2, tức khoảng 34.0 - 0.45
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 34.0f - 0.45f + DEFAULT_OFFSET_C, body.bodyC);
}

void test_warmup_correction_is_capped() {
    BodyTemperatureEstimator estimator(nullptr);
    estimator.begin(0);

    // Tăng rất nhanh 2 °C/phút (bất thường): phần bù không vượt giới hạn
    feed(estimator, 0, MINUTE, [](uint32_t ms) { return 31.0f + 2.0f * ms / 60000.0f; });

    float average = 32.0f;  // Trung bình 0-60 giây
    TEST_ASSERT_FLOAT_WITHIN(0.05f, average + MAX_WARMUP_CORRECTION_C + DEFAULT_OFFSET_C, estimator.current().bodyC);
}

void test_plateau_becomes_stable_with_body_estimate() {
    BodyTemperatureEstimator estimator(nullptr);
    estimator.begin(0);

    feed(estimator, 0, 22 * MINUTE, wearCurve);

    const BodyTemperature& body = estimator.current();
    TEST_ASSERT_EQUAL(BodyTemperatureState::Stable, body.state);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 34.17f + DEFAULT_OFFSET_C, body.bodyC);
}

void test_small_noise_stays_stable() {
    BodyTemperatureEstimator estimator(nullptr);
    estimator.begin(0);

    // ±0.02 °C như quan sát khi gõ phím
    feed(estimator, 0, 6 * MINUTE, [](uint32_t ms) { return (ms / 10000) % 2 ? 34.19f : 34.15f; });

    TEST_ASSERT_EQUAL(BodyTemperatureState::Stable, estimator.current().state);
    TEST_ASSERT_FLOAT_WITHIN(0.03f, 34.17f + DEFAULT_OFFSET_C, estimator.current().bodyC);
}

void test_taking_off_resets_and_requires_new_window() {
    BodyTemperatureEstimator estimator(nullptr);
    estimator.begin(0);
    feed(estimator, 0, 5 * MINUTE, [](uint32_t) { return 34.17f; });
    TEST_ASSERT_TRUE(estimator.current().stable());

    feed(estimator, 5 * MINUTE + 1000, 6 * MINUTE, [](uint32_t) { return 27.0f; });
    TEST_ASSERT_EQUAL(BodyTemperatureState::NotWorn, estimator.current().state);

    // Đeo lại: đo lại từ đầu, chưa đủ 1 phút thì chưa có số
    feed(estimator, 6 * MINUTE + 1000, 7 * MINUTE, [](uint32_t) { return 34.17f; });
    TEST_ASSERT_EQUAL(BodyTemperatureState::Measuring, estimator.current().state);
}

void test_calibrate_requires_stable() {
    FakeConfig config;
    BodyTemperatureEstimator estimator(&config);
    estimator.begin(0);
    feed(estimator, 0, 2 * MINUTE, [](uint32_t) { return 34.17f; });

    // Đã có số tạm thời nhưng chưa cân bằng: không cho hiệu chuẩn
    TEST_ASSERT_TRUE(estimator.current().hasEstimate());
    TEST_ASSERT_FALSE(estimator.calibrate(36.4f));
    TEST_ASSERT_FALSE(config.hasOffset);
}

void test_calibrate_with_oral_reference() {
    FakeConfig config;
    BodyTemperatureEstimator estimator(&config);
    estimator.begin(0);
    feed(estimator, 0, 5 * MINUTE, [](uint32_t) { return 34.17f; });

    // Số đo 22:05: miệng 36.4, cổ tay 34.17
    TEST_ASSERT_TRUE(estimator.calibrate(36.4f));

    const BodyTemperature& body = estimator.current();
    TEST_ASSERT_TRUE(body.calibrated);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.23f, body.offsetC);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 36.4f, body.bodyC);
    TEST_ASSERT_TRUE(config.hasOffset);
    TEST_ASSERT_EQUAL_INT16(223, config.offsetX100);
}

void test_rejects_implausible_offset() {
    BodyTemperatureEstimator estimator(nullptr);
    estimator.begin(0);
    feed(estimator, 0, 5 * MINUTE, [](uint32_t) { return 34.17f; });

    TEST_ASSERT_FALSE(estimator.calibrate(30.0f));   // độ lệch âm
    TEST_ASSERT_FALSE(estimator.calibrate(45.0f));   // độ lệch > 8 °C
    TEST_ASSERT_FALSE(estimator.current().calibrated);
}

void test_offset_is_loaded_and_reset() {
    FakeConfig config;
    config.hasOffset = true;
    config.offsetX100 = 250;

    BodyTemperatureEstimator estimator(&config);
    estimator.begin(0);
    TEST_ASSERT_TRUE(estimator.current().calibrated);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, estimator.current().offsetC);

    estimator.resetOffset();
    TEST_ASSERT_FALSE(estimator.current().calibrated);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, DEFAULT_OFFSET_C, estimator.current().offsetC);
    TEST_ASSERT_FALSE(config.hasOffset);
}

void test_temperature_flags() {
    BodyTemperature body;
    TEST_ASSERT_EQUAL_UINT8(
        LogFlags::TEMP_INVALID | LogFlags::TEMP_UNSTABLE | LogFlags::TEMP_UNCALIBRATED,
        temperatureFlags(false, body)
    );

    body.state = BodyTemperatureState::NotWorn;
    body.calibrated = true;
    TEST_ASSERT_EQUAL_UINT8(LogFlags::TEMP_UNSTABLE | LogFlags::LOOSE, temperatureFlags(true, body));

    body.state = BodyTemperatureState::Stable;
    TEST_ASSERT_EQUAL_UINT8(0, temperatureFlags(true, body));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_starts_unavailable_with_default_offset);
    RUN_TEST(test_cold_skin_is_not_worn);
    RUN_TEST(test_sensor_error_is_unavailable);
    RUN_TEST(test_estimate_after_one_minute_stable_after_three);
    RUN_TEST(test_rising_skin_is_not_stable_but_predicted);
    RUN_TEST(test_real_wear_first_minutes_are_close_to_equilibrium);
    RUN_TEST(test_cooling_skin_is_not_corrected);
    RUN_TEST(test_warmup_correction_is_capped);
    RUN_TEST(test_plateau_becomes_stable_with_body_estimate);
    RUN_TEST(test_small_noise_stays_stable);
    RUN_TEST(test_taking_off_resets_and_requires_new_window);
    RUN_TEST(test_calibrate_requires_stable);
    RUN_TEST(test_calibrate_with_oral_reference);
    RUN_TEST(test_rejects_implausible_offset);
    RUN_TEST(test_offset_is_loaded_and_reset);
    RUN_TEST(test_temperature_flags);
    return UNITY_END();
}
