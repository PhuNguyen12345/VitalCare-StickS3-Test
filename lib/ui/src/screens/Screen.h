#pragma once

#include <M5GFX.h>
#include <stdio.h>

#include "Theme.h"
#include "app/AppViewModel.h"

class Screen {
public:
    virtual ~Screen() = default;

    // Tên trang, dùng cho tên file ảnh chụp
    virtual const char* name() const = 0;
    virtual void draw(M5Canvas& canvas, const AppViewModel& model) = 0;

protected:
    // Hàng trên cùng của mọi trang: tiêu đề bên trái, Bluetooth và pin bên phải
    static void drawHeader(
        M5Canvas& canvas,
        const char* title,
        uint16_t color,
        const DeviceStatus& status
    ) {
        canvas.setTextDatum(top_left);
        canvas.setTextSize(Theme::FONT_SMALL);
        canvas.setTextColor(color, Theme::BACKGROUND);

        canvas.drawString(
            title,
            HEADER_MARGIN,
            HEADER_TEXT_Y
        );

        int32_t left = drawBattery(
            canvas,
            status,
            canvas.width() - HEADER_MARGIN
        );

        if (status.bleConnected) {
            drawBluetooth(
                canvas,
                status.syncing ? Theme::BLE_SYNCING : Theme::BLE_CONNECTED,
                left - 9
            );
        }
    }

    static void drawFooter(M5Canvas& canvas, const char* text) {
        canvas.setTextDatum(bottom_center);
        canvas.setTextSize(Theme::FONT_SMALL);
        canvas.setTextColor(Theme::FOOTER, Theme::BACKGROUND);

        canvas.drawString(
            text,
            canvas.width() / 2,
            canvas.height() - 4
        );
    }

    static void drawCentered(
        M5Canvas& canvas,
        const char* text,
        uint16_t color,
        float size,
        int32_t offsetY = 0
    ) {
        canvas.setTextDatum(middle_center);
        canvas.setTextColor(color, Theme::BACKGROUND);
        canvas.setTextSize(size);

        canvas.drawString(
            text,
            canvas.width() / 2,
            canvas.height() / 2 + offsetY
        );
    }

private:
    static constexpr int32_t HEADER_MARGIN = 4;
    static constexpr int32_t HEADER_TEXT_Y = 6;
    static constexpr int32_t HEADER_CENTER_Y = 11;

    // Vẽ từ phải sang trái, trả về mép trái của cụm pin
    static int32_t drawBattery(
        M5Canvas& canvas,
        const DeviceStatus& status,
        int32_t right
    ) {
        constexpr int32_t BODY_WIDTH = 18;
        constexpr int32_t BODY_HEIGHT = 9;
        constexpr int32_t CAP_WIDTH = 2;
        constexpr int32_t CAP_HEIGHT = 5;
        constexpr int32_t TEXT_GAP = 4;

        const bool known = status.batteryLevel != DeviceStatus::BATTERY_UNKNOWN;

        uint16_t color = TFT_WHITE;

        if (!known) {
            color = TFT_DARKGREY;
        } else if (status.charging) {
            color = TFT_GREEN;
        } else if (status.batteryLevel <= Theme::BATTERY_CRITICAL) {
            color = TFT_RED;
        } else if (status.batteryLevel <= Theme::BATTERY_LOW) {
            color = TFT_YELLOW;
        }

        char text[8];

        if (known) {
            snprintf(text, sizeof(text), "%u%%", status.batteryLevel);
        } else {
            snprintf(text, sizeof(text), "--%%");
        }

        canvas.setTextDatum(top_right);
        canvas.setTextSize(Theme::FONT_SMALL);
        canvas.setTextColor(color, Theme::BACKGROUND);
        canvas.drawString(text, right, HEADER_TEXT_Y);

        const int32_t capX = right - canvas.textWidth(text) - TEXT_GAP - CAP_WIDTH;
        const int32_t bodyX = capX - BODY_WIDTH;
        const int32_t bodyY = HEADER_CENTER_Y - BODY_HEIGHT / 2;

        canvas.drawRect(bodyX, bodyY, BODY_WIDTH, BODY_HEIGHT, color);
        canvas.fillRect(
            capX,
            HEADER_CENTER_Y - CAP_HEIGHT / 2,
            CAP_WIDTH,
            CAP_HEIGHT,
            color
        );

        if (known) {
            const int32_t innerWidth = BODY_WIDTH - 4;
            int32_t fillWidth = innerWidth * status.batteryLevel / 100;

            // Còn pin thì luôn có ít nhất một vạch
            if (status.batteryLevel > 0 && fillWidth == 0) {
                fillWidth = 1;
            }

            canvas.fillRect(bodyX + 2, bodyY + 2, fillWidth, BODY_HEIGHT - 4, color);
        }

        if (status.charging) {
            drawChargingBolt(canvas, bodyX + BODY_WIDTH / 2, HEADER_CENTER_Y);
        }

        return bodyX;
    }

    // Tia sét đè lên thân pin, có viền nền để đọc được trên phần đã tô
    static void drawChargingBolt(M5Canvas& canvas, int32_t x, int32_t y) {
        canvas.fillRect(x - 3, y - 3, 7, 7, Theme::BACKGROUND);
        canvas.drawLine(x + 1, y - 3, x - 2, y + 0, TFT_YELLOW);
        canvas.drawLine(x - 2, y + 0, x + 2, y + 0, TFT_YELLOW);
        canvas.drawLine(x + 2, y + 0, x - 1, y + 3, TFT_YELLOW);
    }

    // Ký hiệu Bluetooth, cao 11 px
    static void drawBluetooth(M5Canvas& canvas, uint16_t color, int32_t x) {
        const int32_t top = HEADER_CENTER_Y - 5;
        const int32_t bottom = HEADER_CENTER_Y + 5;

        canvas.drawLine(x, top, x, bottom, color);
        canvas.drawLine(x, top, x + 3, top + 3, color);
        canvas.drawLine(x + 3, top + 3, x - 3, bottom - 3, color);
        canvas.drawLine(x - 3, top + 3, x + 3, bottom - 3, color);
        canvas.drawLine(x + 3, bottom - 3, x, bottom, color);
    }
};
