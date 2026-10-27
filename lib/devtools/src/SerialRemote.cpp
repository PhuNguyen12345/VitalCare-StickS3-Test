#include "SerialRemote.h"

#if VC_ENABLE_DEVTOOLS

SerialRemote::SerialRemote(Stream& serial, SerialMirror& mirror)
    : serial_(serial), mirror_(mirror) {}

bool SerialRemote::poll(AppEvent& out) {
    while (serial_.available() > 0) {
        char command = (char)serial_.read();

        switch (command) {
            // Giả lập nút: xử lý y như nút thật
            case 'n':
                out = AppEvent::BtnAClick;
                return true;

            case 'c':
                out = AppEvent::BtnBClick;
                return true;

            case 'N':
                out = AppEvent::BtnAHold;
                return true;

            case 'C':
                out = AppEvent::BtnBHold;
                return true;

            // Mirror liên tục
            case 'M':
                mirror_.setMirrorEnabled(true);
                serial_.println("Mirror: ON");
                break;

            case 'm':
                mirror_.setMirrorEnabled(false);
                serial_.println("Mirror: OFF");
                break;

            // Chụp frame kế tiếp
            case 's':
                mirror_.requestCapture();
                break;

            // Bật/tắt tự chụp khi chuyển trang
            case 'a':
                serial_.printf(
                    "Auto capture: %s\n",
                    mirror_.toggleAutoCapture() ? "ON" : "OFF"
                );
                break;

            // Dải màu kiểm tra RGB565
            case 't':
                mirror_.requestCapture();

                if (testPatternHandler_ != nullptr) {
                    testPatternHandler_();
                }
                break;

            default:
                break;
        }
    }

    return false;
}

#endif
