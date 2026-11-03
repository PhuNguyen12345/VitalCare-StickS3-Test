#pragma once

#include <Preferences.h>

#include "ports/IConfigRepository.h"

class PreferencesConfigRepository : public IConfigRepository {
public:
    bool begin() override;

    int16_t tzMinutes(int16_t defaultValue) override;
    void setTzMinutes(int16_t minutes) override;

    bool tempOffsetX100(int16_t& out) override;
    void setTempOffsetX100(int16_t value) override;
    void clearTempOffset() override;

private:
    Preferences preferences_;
    bool ready_ = false;
};
