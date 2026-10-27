#pragma once

#include "screens/Screen.h"

class BootScreen : public Screen {
public:
    const char* name() const override { return "boot"; }

    void draw(M5Canvas& canvas, const AppViewModel&) override {
        canvas.fillSprite(Theme::BACKGROUND);

        drawCentered(canvas, "Starting...", TFT_WHITE, Theme::FONT_MESSAGE);
    }
};
