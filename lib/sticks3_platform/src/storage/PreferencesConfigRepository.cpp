#include "storage/PreferencesConfigRepository.h"

namespace {

constexpr const char* NAMESPACE = "vitalcare";
constexpr const char* KEY_TZ_MINUTES = "tz_min";
constexpr const char* KEY_TEMP_OFFSET = "t_ofs";

// Giá trị lưu không bao giờ bằng mức này: dùng để biết chưa có key
constexpr int16_t MISSING = INT16_MIN;

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

bool PreferencesConfigRepository::tempOffsetX100(int16_t& out) {
    if (!ready_) {
        return false;
    }

    int16_t value = preferences_.getShort(KEY_TEMP_OFFSET, MISSING);

    if (value == MISSING) {
        return false;
    }

    out = value;
    return true;
}

void PreferencesConfigRepository::setTempOffsetX100(int16_t value) {
    if (ready_ && preferences_.getShort(KEY_TEMP_OFFSET, MISSING) != value) {
        preferences_.putShort(KEY_TEMP_OFFSET, value);
    }
}

void PreferencesConfigRepository::clearTempOffset() {
    if (ready_) {
        preferences_.remove(KEY_TEMP_OFFSET);
    }
}
