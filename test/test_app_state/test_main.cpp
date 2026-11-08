#include <unity.h>

#include "app/App.h"

void setUp() {}
void tearDown() {}

static void assertPage(ScreenPage expected, const App& app) {
    TEST_ASSERT_EQUAL_STRING(screenPageName(expected), screenPageName(app.page()));
}

void test_starts_on_clock_and_requests_redraw() {
    App app(nullptr, nullptr);
    app.begin(0);

    assertPage(ScreenPage::Clock, app);
    TEST_ASSERT_TRUE(app.consumeRedrawRequest());
    TEST_ASSERT_FALSE(app.consumeRedrawRequest());
}

void test_a_click_cycles_measurement_pages() {
    App app(nullptr, nullptr);
    app.begin(0);

    app.handleEvent(AppEvent::BtnAClick);
    assertPage(ScreenPage::Temperature, app);

    app.handleEvent(AppEvent::BtnAClick);
    assertPage(ScreenPage::HeartRate, app);

    app.handleEvent(AppEvent::BtnAClick);
    assertPage(ScreenPage::SpO2, app);

    // Sau SpO2 quay lại Temperature, không về Clock
    app.handleEvent(AppEvent::BtnAClick);
    assertPage(ScreenPage::Temperature, app);
}

void test_b_click_returns_to_clock() {
    App app(nullptr, nullptr);
    app.begin(0);

    app.handleEvent(AppEvent::BtnAClick);
    app.handleEvent(AppEvent::BtnAClick);
    app.consumeRedrawRequest();

    app.handleEvent(AppEvent::BtnBClick);
    assertPage(ScreenPage::Clock, app);
    TEST_ASSERT_TRUE(app.consumeRedrawRequest());
}

void test_hold_does_not_change_page() {
    App app(nullptr, nullptr);
    app.begin(0);
    app.handleEvent(AppEvent::BtnAClick);
    app.consumeRedrawRequest();

    app.handleEvent(AppEvent::BtnAHold);
    app.handleEvent(AppEvent::BtnBHold);

    assertPage(ScreenPage::Temperature, app);
    TEST_ASSERT_FALSE(app.consumeRedrawRequest());
}

void test_battery_unknown_until_first_reading() {
    App app(nullptr, nullptr);

    TEST_ASSERT_EQUAL_UINT8(DeviceStatus::BATTERY_UNKNOWN, app.viewModel().status.batteryLevel);
    TEST_ASSERT_FALSE(app.viewModel().status.charging);
}

void test_battery_reading_reaches_view_model() {
    App app(nullptr, nullptr);

    app.setBattery(87, true);

    TEST_ASSERT_EQUAL_UINT8(87, app.viewModel().status.batteryLevel);
    TEST_ASSERT_TRUE(app.viewModel().status.charging);

    // Lỗi đọc pin không được hiện thành 0%
    app.setBattery(DeviceStatus::BATTERY_UNKNOWN, false);

    TEST_ASSERT_EQUAL_UINT8(DeviceStatus::BATTERY_UNKNOWN, app.viewModel().status.batteryLevel);
    TEST_ASSERT_FALSE(app.viewModel().status.charging);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_starts_on_clock_and_requests_redraw);
    RUN_TEST(test_a_click_cycles_measurement_pages);
    RUN_TEST(test_b_click_returns_to_clock);
    RUN_TEST(test_hold_does_not_change_page);
    RUN_TEST(test_battery_unknown_until_first_reading);
    RUN_TEST(test_battery_reading_reaches_view_model);
    return UNITY_END();
}
