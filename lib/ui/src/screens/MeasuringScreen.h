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
            drawTemperature(canvas, model);
        } else {
            drawBio(canvas, model);
        }

        drawFooter(canvas, "A: Next   B: Clock");
    }

private:
    // Nhiệt độ cơ thể ước tính (xem BodyTemperatureEstimator), kèm nhiệt độ da thô
    void drawTemperature(M5Canvas& canvas, const AppViewModel& model) {
        const BodyTemperature& body = model.bodyTemperature;

        drawTitle(canvas, "BODY TEMPERATURE", TFT_YELLOW);

        if (!model.reading.temperatureValid) {
            drawCentered(canvas, "MAX ERROR", TFT_RED, Theme::FONT_MESSAGE);
            return;
        }

        char detailText[32];

        if (body.state == BodyTemperatureState::NotWorn) {
            drawCentered(canvas, "NOT WORN", TFT_YELLOW, Theme::FONT_MESSAGE, -8);

            snprintf(detailText, sizeof(detailText), "Skin %.1f C", model.reading.temperatureC);
            drawCentered(canvas, detailText, TFT_WHITE, Theme::FONT_SMALL, 22);
            return;
        }

        // Đang đeo nhưng chưa cân bằng nhiệt: chưa hiện số
        if (!body.stable()) {
            drawCentered(canvas, "--.- C", TFT_DARKGREY, Theme::FONT_VALUE, -7);

            snprintf(detailText, sizeof(detailText), "Stabilizing (%.1f C)", model.reading.temperatureC);
            drawCentered(canvas, detailText, TFT_YELLOW, Theme::FONT_SMALL, 27);
            return;
        }

        // "~": nhiệt độ ước tính, làm tròn 0.1 °C
        char valueText[20];
        snprintf(valueText, sizeof(valueText), "~%.1f C", body.bodyC);
        drawCentered(canvas, valueText, TFT_GREEN, Theme::FONT_VALUE, -7);

        snprintf(
            detailText,
            sizeof(detailText),
            body.calibrated ? "Skin %.2f C" : "Skin %.2f C, uncal.",
            model.reading.temperatureC
        );
        drawCentered(canvas, detailText, body.calibrated ? TFT_WHITE : TFT_DARKGREY, Theme::FONT_SMALL, 27);
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

        // Cùng quy tắc với gói live và log CSV
        BioSampleQuality quality = evaluateBioSample(reading);

        if (!quality.fingerDetected) {
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

        // Có ngón tay nhưng chưa đạt ngưỡng: chưa hiện số
        if (!(isHeartRate ? quality.heartRateValid : quality.spo2Valid)) {
            drawCentered(canvas, isHeartRate ? "-- BPM" : "-- %", TFT_DARKGREY, Theme::FONT_VALUE, -7);
            drawCentered(canvas, "Measuring...", TFT_YELLOW, Theme::FONT_SMALL, 27);
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

        // confidence là độ tin cậy của nhịp tim, ghi rõ khi ở trang SpO2
        snprintf(
            confidenceText,
            sizeof(confidenceText),
            isHeartRate ? "Confidence: %d%%" : "HR conf: %d%%",
            reading.confidence
        );

        drawCentered(canvas, confidenceText, TFT_GREEN, Theme::FONT_SMALL, 27);
    }

    Metric metric_;
};
