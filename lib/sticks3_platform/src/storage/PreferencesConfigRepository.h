#pragma once

#include <Preferences.h>

#include "ports/IConfigRepository.h"

class PreferencesConfigRepository : public IConfigRepository {
public:
    bool begin() override;

    int16_t tzMinutes(int16_t defaultValue) override;
    void setTzMinutes(int16_t minutes) override;

private:
    Preferences preferences_;
    bool ready_ = false;
};
