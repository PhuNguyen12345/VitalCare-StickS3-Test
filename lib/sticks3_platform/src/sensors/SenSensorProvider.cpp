#include "sensors/SenSensorProvider.h"

SenSensorProvider::SenSensorProvider(
    I2cBusManager& bus,
    uint8_t sda,
    uint8_t scl,
    uint8_t resetPin,
    uint8_t mfioPin
)
    : bus_(bus),
      sda_(sda),
      scl_(scl),
      resetPin_(resetPin),
      mfioPin_(mfioPin),
      // Constructor nhận RESET trước, MFIO sau
      bioHub_(resetPin, mfioPin) {}

bool SenSensorProvider::begin() {
    initialized_ = false;

    Serial.println("\nStarting SEN-15219...");
    Serial.printf(
        "SDA=G%d, SCL=G%d, RESET=G%d, MFIO=G%d\n",
        sda_,
        scl_,
        resetPin_,
        mfioPin_
    );

    if (!bus_.select(sda_, scl_)) {
        Serial.println("Cannot start SEN I2C bus");
        return false;
    }

    delay(100);

    // Quan trọng: liên kết thư viện với bus Wire hiện tại
    uint8_t beginResult = bioHub_.begin(
        bus_.wire(),
        resetPin_,
        mfioPin_
    );

    Serial.printf("bioHub.begin result: %u\n", beginResult);

    if (beginResult != 0) {
        Serial.println("SEN-15219 begin FAILED");
        return false;
    }

    delay(500);

    uint8_t configResult = bioHub_.configBpm(MODE_ONE);

    Serial.printf("configBpm result: %u\n", configResult);

    if (configResult != 0) {
        Serial.println("SEN-15219 config FAILED");
        return false;
    }

    delay(4000);

    uint8_t sensorState = bioHub_.readMAX30101State();

    Serial.printf(
        "MAX30101 state: %u\n",
        sensorState
    );

    initialized_ = true;
    Serial.println("SEN-15219 initialized successfully");

    return true;
}

bool SenSensorProvider::read(HealthReading& out) {
    if (!initialized_) {
        return false;
    }

    if (!bus_.select(sda_, scl_)) {
        out.bioValid = false;
        return false;
    }

    bioData body = bioHub_.readBpm();

    out.heartRate = body.heartRate;
    out.spo2 = body.oxygen;
    out.confidence = body.confidence;
    out.fingerStatus = body.status;
    out.bioValid = true;

    return true;
}

void SenSensorProvider::reportStatus(DeviceStatus& status) const {
    status.senInitialized = initialized_;
}
