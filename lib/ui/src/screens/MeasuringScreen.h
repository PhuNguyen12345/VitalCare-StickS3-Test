#pragma once

#include <stdio.h>

#include "screens/Screen.h"

// Một lớp dùng chung cho ba trang đo: nhiệt độ, nhịp tim, SpO2
class MeasuringScreen : public Screen {
public:
    enum class Metric : uint8_t {
        Temperature,
        HeartRate,
        SpO2
    };

    explicit MeasuringScreen(Metric metric) : metric_(metric) {}

    const char* name() const override {
        switch (metric_) {
            case Metric::Temperature:
                return "temperature";

            case Metric::HeartRate:
                return "heart_rate";

            case Metric::SpO2:
                return "spo2";
        }

        return "unknown";
    }

    void draw(M5Canvas& canvas, const AppViewModel& model) override {
        canvas.fillSprite(Theme::BACKGROUND);

        if (metric_ == Metric::Temperature) {
            drawTemperature(canvas, model.reading);
        } else {
            drawBio(canvas, model);
        }

        drawFooter(canvas, "A: Next   B: Clock");
    }

private:
    void drawTemperature(M5Canvas& canvas, const HealthReading& reading) {
        drawTitle(canvas, "SKIN TEMPERATURE", TFT_YELLOW);

        if (reading.temperatureValid) {
            char temperatureText[20];

            snprintf(
                temperatureText,
                sizeof(temperatureText),
                "%.2f C",
                reading.temperatureC
            );

            drawCentered(canvas, temperatureText, TFT_GREEN, Theme::FONT_VALUE);
        } else {
            drawCentered(canvas, "MAX ERROR", TFT_RED, Theme::FONT_MESSAGE);
        }
    }

    // Nhịp tim và SpO2 chỉ khác tiêu đề, gợi ý và định dạng giá trị
    void drawBio(M5Canvas& canvas, const AppViewModel& model) {
        const bool isHeartRate = metric_ == Metric::HeartRate;
        const HealthReading& reading = model.reading;

        if (isHeartRate) {
            drawTitle(canvas, "HEART RATE", TFT_RED);
        } else {
            drawTitle(canvas, "BLOOD OXYGEN", TFT_CYAN);
        }

        if (!model.status.senInitialized) {
            drawCentered(canvas, "SEN ERROR", TFT_RED, Theme::FONT_MESSAGE);
            return;
        }

        // Status 3: phát hiện ngón tay
        if (!reading.bioValid || reading.fingerStatus != 3) {
            drawCentered(canvas, "PLACE FINGER", TFT_YELLOW, Theme::FONT_MESSAGE, -8);

            drawCentered(
                canvas,
                isHeartRate ? "Light, steady pressure" : "Wait for stable signal",
                TFT_WHITE,
                Theme::FONT_SMALL,
                22
            );
            return;
        }

        char valueText[20];

        if (isHeartRate) {
            snprintf(valueText, sizeof(valueText), "%.1f BPM", (double)reading.heartRate);
        } else {
            snprintf(valueText, sizeof(valueText), "%.1f %%", (double)reading.spo2);
        }

        drawCentered(canvas, valueText, TFT_WHITE, Theme::FONT_VALUE, -7);

        char confidenceText[24];

        snprintf(
            confidenceText,
            sizeof(confidenceText),
            "Confidence: %d%%",
            reading.confidence
        );

        drawCentered(canvas, confidenceText, TFT_GREEN, Theme::FONT_SMALL, 27);
    }

    Metric metric_;
};
