#include <unity.h>

#include "domain/HealthReading.h"

void setUp() {}
void tearDown() {}

static HealthReading finger(uint16_t hr, uint16_t spo2, uint8_t confidence) {
    HealthReading reading;
    reading.bioValid = true;
    reading.fingerStatus = 3;
    reading.heartRate = hr;
    reading.spo2 = spo2;
    reading.confidence = confidence;
    return reading;
}

void test_no_finger() {
    HealthReading reading = finger(70, 98, 90);
    reading.fingerStatus = 2;

    BioSampleQuality quality = evaluateBioSample(reading);
    TEST_ASSERT_FALSE(quality.fingerDetected);
    TEST_ASSERT_FALSE(quality.heartRateValid);
    TEST_ASSERT_FALSE(quality.spo2Valid);
}

void test_not_read_yet() {
    HealthReading reading = finger(70, 98, 90);
    reading.bioValid = false;

    TEST_ASSERT_FALSE(evaluateBioSample(reading).fingerDetected);
}

void test_valid_sample() {
    BioSampleQuality quality = evaluateBioSample(finger(70, 98, 90));
    TEST_ASSERT_TRUE(quality.fingerDetected);
    TEST_ASSERT_TRUE(quality.heartRateValid);
    TEST_ASSERT_TRUE(quality.spo2Valid);
}

void test_confidence_threshold() {
    uint8_t threshold = SampleThresholds::MIN_CONFIDENCE;

    TEST_ASSERT_TRUE(evaluateBioSample(finger(70, 98, threshold)).heartRateValid);

    BioSampleQuality below = evaluateBioSample(finger(70, 98, threshold - 1));
    TEST_ASSERT_TRUE(below.fingerDetected);
    TEST_ASSERT_FALSE(below.heartRateValid);
    TEST_ASSERT_FALSE(below.spo2Valid);  // SpO2 cũng không tin khi nhịp tim chưa tin
}

void test_spo2_zero_or_out_of_range() {
    BioSampleQuality zero = evaluateBioSample(finger(62, 0, 98));
    TEST_ASSERT_TRUE(zero.heartRateValid);
    TEST_ASSERT_FALSE(zero.spo2Valid);

    TEST_ASSERT_FALSE(evaluateBioSample(finger(62, 101, 98)).spo2Valid);
    TEST_ASSERT_TRUE(evaluateBioSample(finger(62, 100, 98)).spo2Valid);
}

void test_zero_heart_rate() {
    TEST_ASSERT_FALSE(evaluateBioSample(finger(0, 98, 98)).heartRateValid);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_no_finger);
    RUN_TEST(test_not_read_yet);
    RUN_TEST(test_valid_sample);
    RUN_TEST(test_confidence_threshold);
    RUN_TEST(test_spo2_zero_or_out_of_range);
    RUN_TEST(test_zero_heart_rate);
    return UNITY_END();
}
