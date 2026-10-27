#pragma once

#include <M5GFX.h>

#include "Theme.h"
#include "app/AppViewModel.h"

class Screen {
public:
    virtual ~Screen() = default;

    // Tên trang, dùng cho tên file ảnh chụp
    virtual const char* name() const = 0;
    virtual void draw(M5Canvas& canvas, const AppViewModel& model) = 0;

protected:
    static void drawTitle(M5Canvas& canvas, const char* text, uint16_t color) {
        canvas.setTextDatum(top_center);
        canvas.setTextSize(Theme::FONT_SMALL);
        canvas.setTextColor(color, Theme::BACKGROUND);

        canvas.drawString(
            text,
            canvas.width() / 2,
            6
        );
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
};
