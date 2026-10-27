#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "ports/IConnectivity.h"

// GATT server NimBLE: live / control / sync / status
class BleManager : public IConnectivity {
public:
    struct Uuids {
        const char* service;
        const char* live;
        const char* control;
        const char* sync;
        const char* status;
    };

    BleManager(const char* namePrefix, const Uuids& uuids, uint16_t preferredMtu);

    bool begin() override;
    bool isConnected() const override;
    size_t syncPayloadSize() const override;

    bool publishLive(const uint8_t* data, size_t length) override;
    bool publishStatus(const uint8_t* data, size_t length) override;
    bool sendSync(const uint8_t* data, size_t length) override;

    bool poll(BleProtocol::ControlCommand& out) override;

    const char* deviceName() const { return deviceName_; }

private:
    class ServerCallbacks;
    class ControlCallbacks;

    // Lệnh ghi từ điện thoại, đưa vào hàng đợi để loop xử lý
    struct ControlMessage {
        uint8_t length;
        uint8_t data[20];
    };

    static constexpr uint16_t DEFAULT_MTU = 23;
    static constexpr uint8_t CONTROL_QUEUE_LENGTH = 8;

    const char* namePrefix_;
    Uuids uuids_;
    uint16_t preferredMtu_;

    char deviceName_[24] = {0};

    NimBLEServer* server_ = nullptr;
    NimBLECharacteristic* live_ = nullptr;
    NimBLECharacteristic* sync_ = nullptr;
    NimBLECharacteristic* status_ = nullptr;

    QueueHandle_t controlQueue_ = nullptr;
    volatile uint16_t mtu_ = DEFAULT_MTU;
};
