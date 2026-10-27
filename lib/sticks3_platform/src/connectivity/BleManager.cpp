#include "connectivity/BleManager.h"

// ======================================================
// Callback (chạy trong task của NimBLE, không phải loop)
// ======================================================

class BleManager::ServerCallbacks : public NimBLEServerCallbacks {
public:
    explicit ServerCallbacks(BleManager& owner) : owner_(owner) {}

    void onConnect(NimBLEServer*, NimBLEConnInfo&) override {
        owner_.mtu_ = DEFAULT_MTU;
    }

    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
        owner_.mtu_ = DEFAULT_MTU;
    }

    void onMTUChange(uint16_t mtu, NimBLEConnInfo&) override {
        owner_.mtu_ = mtu;
    }

private:
    BleManager& owner_;
};

class BleManager::ControlCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit ControlCallbacks(BleManager& owner) : owner_(owner) {}

    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        NimBLEAttValue value = characteristic->getValue();

        ControlMessage message;
        message.length = (uint8_t)std::min<size_t>(value.size(), sizeof(message.data));
        memcpy(message.data, value.data(), message.length);

        xQueueSend(owner_.controlQueue_, &message, 0);
    }

private:
    BleManager& owner_;
};

// ======================================================
// BleManager
// ======================================================

BleManager::BleManager(const char* namePrefix, const Uuids& uuids, uint16_t preferredMtu)
    : namePrefix_(namePrefix), uuids_(uuids), preferredMtu_(preferredMtu) {}

bool BleManager::begin() {
    controlQueue_ = xQueueCreate(CONTROL_QUEUE_LENGTH, sizeof(ControlMessage));

    // Tên theo 2 byte cuối địa chỉ MAC để phân biệt nhiều thiết bị
    uint64_t mac = ESP.getEfuseMac();
    snprintf(
        deviceName_,
        sizeof(deviceName_),
        "%s%02X%02X",
        namePrefix_,
        (uint8_t)(mac >> 32),
        (uint8_t)(mac >> 40)
    );

    if (!NimBLEDevice::init(deviceName_)) {
        Serial.println("BLE init FAILED");
        return false;
    }

    NimBLEDevice::setMTU(preferredMtu_);

    server_ = NimBLEDevice::createServer();
    server_->setCallbacks(new ServerCallbacks(*this));
    server_->advertiseOnDisconnect(true);

    NimBLEService* service = server_->createService(uuids_.service);

    live_ = service->createCharacteristic(
        uuids_.live,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    NimBLECharacteristic* control = service->createCharacteristic(
        uuids_.control,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    control->setCallbacks(new ControlCallbacks(*this));

    sync_ = service->createCharacteristic(
        uuids_.sync,
        NIMBLE_PROPERTY::NOTIFY
    );

    status_ = service->createCharacteristic(
        uuids_.status,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // UUID 128-bit trong gói quảng bá, tên trong scan response.
    // Service tự khởi động khi bắt đầu quảng bá.
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(uuids_.service);
    advertising->enableScanResponse(true);
    advertising->setName(deviceName_);

    bool started = advertising->start();

    Serial.printf("BLE: advertising as %s %s\n", deviceName_, started ? "OK" : "FAILED");

    return started;
}

bool BleManager::isConnected() const {
    return server_ != nullptr && server_->getConnectedCount() > 0;
}

size_t BleManager::syncPayloadSize() const {
    return mtu_ - 3;
}

bool BleManager::publishLive(const uint8_t* data, size_t length) {
    if (live_ == nullptr) {
        return false;
    }

    live_->setValue(data, length);
    return !isConnected() || live_->notify();
}

bool BleManager::publishStatus(const uint8_t* data, size_t length) {
    if (status_ == nullptr) {
        return false;
    }

    status_->setValue(data, length);
    return !isConnected() || status_->notify();
}

bool BleManager::sendSync(const uint8_t* data, size_t length) {
    if (sync_ == nullptr || !isConnected()) {
        return false;
    }

    return sync_->notify(data, length);
}

bool BleManager::poll(BleProtocol::ControlCommand& out) {
    ControlMessage message;

    while (controlQueue_ != nullptr && xQueueReceive(controlQueue_, &message, 0) == pdTRUE) {
        if (BleProtocol::decodeControl(message.data, message.length, out)) {
            return true;
        }

        Serial.println("BLE: invalid control command");
    }

    return false;
}
