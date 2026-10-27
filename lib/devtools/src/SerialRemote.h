#pragma once

#include "BuildConfig.h"

#if VC_ENABLE_DEVTOOLS

#include <Arduino.h>

#include "SerialMirror.h"
#include "ports/IInputSource.h"

// Lệnh một ký tự từ Serial (simulator / canvas_capture.py):
//   n / c = click A / B        N / C = giữ A / B
//   M / m = bật / tắt mirror   s = chụp   a = tự chụp khi đổi trang
//   t = dải màu kiểm tra
class SerialRemote : public IInputSource {
public:
    using ActionFn = void (*)();

    SerialRemote(Stream& serial, SerialMirror& mirror);

    void setTestPatternHandler(ActionFn handler) { testPatternHandler_ = handler; }

    bool poll(AppEvent& out) override;

private:
    Stream& serial_;
    SerialMirror& mirror_;
    ActionFn testPatternHandler_ = nullptr;
};

#endif
