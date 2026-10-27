#include "storage/PreferencesConfigRepository.h"

namespace {

constexpr const char* NAMESPACE = "vitalcare";
constexpr const char* KEY_TZ_MINUTES = "tz_min";

}  // namespace

bool PreferencesConfigRepository::begin() {
    ready_ = preferences_.begin(NAMESPACE, false);
    return ready_;
}

int16_t PreferencesConfigRepository::tzMinutes(int16_t defaultValue) {
    return ready_ ? preferences_.getShort(KEY_TZ_MINUTES, defaultValue) : defaultValue;
}

void PreferencesConfigRepository::setTzMinutes(int16_t minutes) {
    if (ready_ && preferences_.getShort(KEY_TZ_MINUTES, INT16_MIN) != minutes) {
        preferences_.putShort(KEY_TZ_MINUTES, minutes);
    }
}
