#pragma once

#include <stdio.h>

#include "screens/Screen.h"

// Trang đồng hồ
class HomeScreen : public Screen {
public:
    const char* name() const override { return "clock"; }

    void draw(M5Canvas& canvas, const AppViewModel& model) override {
        canvas.fillSprite(Theme::BACKGROUND);

        drawTitle(canvas, "VITALCARE - RTC", TFT_CYAN);
        drawConnection(canvas, model.status);

        if (model.time.valid) {
            char timeText[12];

            snprintf(
                timeText,
                sizeof(timeText),
                "%02d:%02d:%02d",
                model.time.hour,
                model.time.minute,
                model.time.second
            );

            drawCentered(canvas, timeText, TFT_WHITE, Theme::FONT_VALUE);
        } else {
            drawCentered(canvas, "RTC ERROR", TFT_RED, Theme::FONT_MESSAGE);
        }

        drawFooter(canvas, "A: Measurements");
    }

private:
    // Góc trên bên phải: SYNC khi đang đồng bộ, BLE khi đã kết nối
    static void drawConnection(M5Canvas& canvas, const DeviceStatus& status) {
        if (!status.bleConnected) {
            return;
        }

        canvas.setTextDatum(top_right);
        canvas.setTextSize(Theme::FONT_SMALL);
        canvas.setTextColor(status.syncing ? TFT_ORANGE : TFT_GREEN, Theme::BACKGROUND);

        canvas.drawString(
            status.syncing ? "SYNC" : "BLE",
            canvas.width() - 4,
            6
        );
    }
};
