#include "UiController.h"

UiController::UiController(M5GFX& display) : canvas_(&display) {}

bool UiController::begin(int16_t width, int16_t height) {
    canvas_.setColorDepth(16);

    // Cố định kích thước canvas, không phụ thuộc vào việc dò LCD
    if (!canvas_.createSprite(width, height)) {
        return false;
    }

    canvas_.setTextWrap(false);

    return true;
}

void UiController::showBoot() {
    AppViewModel emptyModel;

    bootScreen_.draw(canvas_, emptyModel);
    present(bootScreen_.name());
}

void UiController::render(const AppViewModel& model) {
    Screen& screen = screenFor(model.page);

    screen.draw(canvas_, model);
    present(screen.name());
}

void UiController::showTestPattern() {
    constexpr uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_YELLOW, TFT_CYAN};
    constexpr int16_t bandCount = sizeof(colors) / sizeof(colors[0]);

    int16_t bandWidth = canvas_.width() / bandCount;

    for (int16_t i = 0; i < bandCount; i++) {
        canvas_.fillRect(
            i * bandWidth,
            0,
            bandWidth,
            canvas_.height(),
            colors[i]
        );
    }

    present("test_pattern");
}

Screen& UiController::screenFor(ScreenPage page) {
    switch (page) {
        case ScreenPage::Clock:
            return homeScreen_;

        case ScreenPage::Temperature:
            return temperatureScreen_;

        case ScreenPage::HeartRate:
            return heartRateScreen_;

        case ScreenPage::SpO2:
            return spo2Screen_;
    }

    return homeScreen_;
}

void UiController::present(const char* name) {
    canvas_.pushSprite(0, 0);

    const uint8_t* pixels = (const uint8_t*)canvas_.getBuffer();

    if (frameListener_ != nullptr && pixels != nullptr) {
        frameListener_->onFrame(pixels, canvas_.width(), canvas_.height(), name);
    }
}
