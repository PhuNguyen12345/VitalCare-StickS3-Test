#include <unity.h>

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "services/SyncService.h"

using namespace BleProtocol;
using Packet = std::vector<uint8_t>;

// ======================================================
// Fake
// ======================================================

// Repository trong bộ nhớ, cùng quy ước segment với LittleFsLogRepository
class FakeRepository : public ILogRepository {
public:
    std::map<uint16_t, std::string> segments;
    uint16_t activeId = 1;

    void addClosedSegment(const std::string& content) {
        segments[activeId++] = content;
    }

    void writeActive(const std::string& content) {
        segments[activeId] += content;
    }

    bool begin() override { return true; }
    bool append(const LogRecord&) override { return true; }

    bool rotate() override {
        if (segments.count(activeId) && !segments[activeId].empty()) {
            activeId++;
        }
        return true;
    }

    bool oldestSegment(uint16_t& id, uint32_t& size) override {
        for (auto& entry : segments) {
            if (entry.first < activeId) {
                id = entry.first;
                size = entry.second.size();
                return true;
            }
        }
        return false;
    }

    size_t readSegment(uint16_t id, uint32_t offset, uint8_t* buffer, size_t length) override {
        const std::string& data = segments.at(id);
        if (offset >= data.size()) {
            return 0;
        }
        size_t count = std::min(length, data.size() - offset);
        memcpy(buffer, data.data() + offset, count);
        return count;
    }

    bool deleteSegment(uint16_t id) override { return segments.erase(id) > 0; }
    bool clear() override { segments.clear(); return true; }

    LogStats stats() const override {
        LogStats stats;
        stats.segmentCount = segments.size();
        for (auto& entry : segments) {
            stats.totalBytes += entry.second.size();
        }
        return stats;
    }
};

class FakeLink : public IConnectivity {
public:
    bool connected = true;
    size_t payloadSize = 20;  // MTU mặc định 23
    int failEvery = 0;        // Mỗi failEvery lần gửi sync thì 1 lần "bộ đệm đầy"
    int sendAttempts = 0;

    std::vector<Packet> sync;
    std::vector<Packet> live;
    std::vector<Packet> status;
    std::deque<ControlCommand> commands;

    void command(ControlOp op, uint16_t segmentId = 0) {
        ControlCommand value;
        value.op = op;
        value.segmentId = segmentId;
        commands.push_back(value);
    }

    bool begin() override { return true; }
    bool isConnected() const override { return connected; }
    size_t syncPayloadSize() const override { return payloadSize; }

    bool publishLive(const uint8_t* data, size_t length) override {
        live.emplace_back(data, data + length);
        return true;
    }

    bool publishStatus(const uint8_t* data, size_t length) override {
        status.emplace_back(data, data + length);
        return true;
    }

    bool sendSync(const uint8_t* data, size_t length) override {
        sendAttempts++;
        if (failEvery > 0 && sendAttempts % failEvery == 0) {
            return false;
        }
        sync.emplace_back(data, data + length);
        return true;
    }

    bool poll(ControlCommand& out) override {
        if (commands.empty()) {
            return false;
        }
        out = commands.front();
        commands.pop_front();
        return true;
    }
};

class FakeClock : public ITimeProvider {
public:
    TimeInfo now = fromEpoch(1761550000);
    int writes = 0;

    bool begin() override { return true; }
    bool read(TimeInfo& out) override { out = now; return true; }
    bool write(const TimeInfo& time) override { now = time; writes++; return true; }
};

// ======================================================
// Tiện ích
// ======================================================

struct Fixture {
    FakeRepository repo;
    FakeLink link;
    FakeClock clock;
    TimeService time{clock, nullptr, 1000, 420};
    SyncService sync{link, repo, time, 1000, 5000, 10000};
    HealthReading reading;
    DeviceStatus status;
    uint32_t now = 0;

    Fixture() {
        time.begin(0);
        sync.begin();
    }

    void step(int count = 1) {
        for (int i = 0; i < count; i++) {
            now += 10;
            sync.update(now, reading, status);
        }
    }
};

static uint16_t u16(const Packet& p, size_t at) { return p[at] | (p[at + 1] << 8); }
static uint32_t u32(const Packet& p, size_t at) {
    return p[at] | (p[at + 1] << 8) | (p[at + 2] << 16) | ((uint32_t)p[at + 3] << 24);
}

struct ReceivedSegment {
    uint16_t id = 0;
    std::string data;
    bool crcOk = false;
};

// Ghép các gói sync thành segment, giống việc app điện thoại sẽ làm
static std::vector<ReceivedSegment> collect(const std::vector<Packet>& packets, bool& done) {
    std::vector<ReceivedSegment> segments;
    ReceivedSegment current;
    done = false;

    for (const Packet& p : packets) {
        uint16_t id = u16(p, 0);
        SyncType type = (SyncType)p[4];

        if (type == SyncType::Data) {
            current.id = id;
            current.data.append(p.begin() + SYNC_HEADER_SIZE, p.end());
        } else if (type == SyncType::SegmentEnd) {
            uint32_t total = u32(p, 5);
            uint32_t crc = u32(p, 9);
            current.id = id;
            current.crcOk = total == current.data.size() &&
                crc == crc32Update(0, (const uint8_t*)current.data.data(), current.data.size());
            segments.push_back(current);
            current = ReceivedSegment();
        } else if (type == SyncType::SyncDone) {
            done = true;
        }
    }

    return segments;
}

// Đợi gói SEGMENT_END mới nhất rồi ACK nó
static void runSyncWithAcks(Fixture& f, int maxSteps = 2000) {
    size_t acked = 0;

    for (int i = 0; i < maxSteps; i++) {
        f.step();

        bool done;
        auto segments = collect(f.link.sync, done);

        if (done) {
            return;
        }

        if (segments.size() > acked) {
            f.link.command(ControlOp::SyncAck, segments.back().id);
            acked = segments.size();
        }
    }
}

static std::string csv(int records) {
    std::string data = LOG_CSV_HEADER;
    for (int i = 0; i < records; i++) {
        data += "17615500" + std::to_string(10 + i) + ",3650,80,98,95,0\n";
    }
    return data;
}

void setUp() {}
void tearDown() {}

// ======================================================
// Test
// ======================================================

void test_full_sync_sends_all_segments_and_deletes_after_ack() {
    Fixture f;
    f.repo.addClosedSegment(csv(30));
    f.repo.addClosedSegment(csv(5));
    f.repo.writeActive(csv(3));  // Segment đang ghi: SYNC_START phải đóng nó lại

    f.link.command(ControlOp::SyncStart);
    runSyncWithAcks(f);

    bool done;
    auto segments = collect(f.link.sync, done);

    TEST_ASSERT_TRUE(done);
    TEST_ASSERT_EQUAL(3, segments.size());
    TEST_ASSERT_EQUAL_UINT16(1, segments[0].id);
    TEST_ASSERT_EQUAL_STRING(csv(30).c_str(), segments[0].data.c_str());
    TEST_ASSERT_EQUAL_STRING(csv(3).c_str(), segments[2].data.c_str());

    for (auto& segment : segments) {
        TEST_ASSERT_TRUE(segment.crcOk);
    }

    TEST_ASSERT_EQUAL(0, f.repo.segments.size());
    TEST_ASSERT_FALSE(f.sync.syncing());
}

void test_packets_respect_mtu() {
    Fixture f;
    f.link.payloadSize = 20;
    f.repo.addClosedSegment(csv(10));

    f.link.command(ControlOp::SyncStart);
    runSyncWithAcks(f);

    for (const Packet& p : f.link.sync) {
        TEST_ASSERT_TRUE(p.size() <= 20);
    }
}

void test_buffer_full_retries_without_gaps() {
    Fixture f;
    f.link.failEvery = 3;
    f.repo.addClosedSegment(csv(40));

    f.link.command(ControlOp::SyncStart);
    runSyncWithAcks(f);

    bool done;
    auto segments = collect(f.link.sync, done);

    TEST_ASSERT_TRUE(done);
    TEST_ASSERT_EQUAL(1, segments.size());
    TEST_ASSERT_TRUE(segments[0].crcOk);
    TEST_ASSERT_EQUAL_STRING(csv(40).c_str(), segments[0].data.c_str());
}

void test_disconnect_keeps_unacked_segment() {
    Fixture f;
    f.repo.addClosedSegment(csv(40));

    f.link.command(ControlOp::SyncStart);
    f.step(3);
    TEST_ASSERT_TRUE(f.sync.syncing());

    f.link.connected = false;
    f.step();

    TEST_ASSERT_FALSE(f.sync.syncing());
    TEST_ASSERT_EQUAL(1, f.repo.segments.size());

    // Kết nối lại: gửi lại segment từ đầu, đủ dữ liệu
    f.link.connected = true;
    f.link.sync.clear();
    f.link.command(ControlOp::SyncStart);
    runSyncWithAcks(f);

    bool done;
    auto segments = collect(f.link.sync, done);
    TEST_ASSERT_TRUE(done);
    TEST_ASSERT_TRUE(segments[0].crcOk);
    TEST_ASSERT_EQUAL(0, f.repo.segments.size());
}

void test_ack_timeout_keeps_segment() {
    Fixture f;
    f.repo.addClosedSegment(csv(2));

    f.link.command(ControlOp::SyncStart);
    f.step(100);  // Gửi xong, đang chờ ACK
    TEST_ASSERT_TRUE(f.sync.syncing());

    f.step(1100);  // > 10 giây không ACK
    TEST_ASSERT_FALSE(f.sync.syncing());
    TEST_ASSERT_EQUAL(1, f.repo.segments.size());
}

void test_wrong_ack_is_ignored() {
    Fixture f;
    f.repo.addClosedSegment(csv(2));

    f.link.command(ControlOp::SyncStart);
    f.step(100);
    f.link.command(ControlOp::SyncAck, 99);
    f.step(5);

    TEST_ASSERT_TRUE(f.sync.syncing());
    TEST_ASSERT_EQUAL(1, f.repo.segments.size());
}

void test_empty_log_sends_done_immediately() {
    Fixture f;

    f.link.command(ControlOp::SyncStart);
    f.step(5);

    bool done;
    collect(f.link.sync, done);
    TEST_ASSERT_TRUE(done);
    TEST_ASSERT_FALSE(f.sync.syncing());
}

void test_live_every_second_only_when_connected() {
    Fixture f;
    f.reading.temperatureC = 36.52f;
    f.reading.temperatureValid = true;

    f.step(300);  // 3 giây
    TEST_ASSERT_EQUAL(3, f.link.live.size());

    const Packet& p = f.link.live.back();
    TEST_ASSERT_EQUAL(LIVE_SIZE, p.size());
    TEST_ASSERT_EQUAL_UINT32(1761550000, u32(p, 0));
    TEST_ASSERT_EQUAL_INT16(3652, (int16_t)u16(p, 4));
    TEST_ASSERT_EQUAL_UINT8(LogFlags::BIO_INVALID, p[11]);  // SEN chưa sẵn sàng
    TEST_ASSERT_EQUAL_UINT16(2, u16(p, 12));                // seq

    f.link.connected = false;
    f.step(300);
    TEST_ASSERT_EQUAL(3, f.link.live.size());
}

void test_set_time_writes_rtc_and_timezone() {
    Fixture f;

    ControlCommand command;
    command.op = ControlOp::SetTime;
    command.epoch = 1800000000;
    command.tzMinutes = -300;
    f.link.commands.push_back(command);
    f.step();

    TEST_ASSERT_EQUAL(1, f.clock.writes);
    TEST_ASSERT_EQUAL_UINT32(1800000000, f.time.epochUtc());
    TEST_ASSERT_EQUAL_INT16(-300, f.time.tzMinutes());

    // Status gửi ngay sau khi chỉnh giờ, chứa múi giờ mới
    const Packet& p = f.link.status.back();
    TEST_ASSERT_EQUAL_INT16(-300, (int16_t)u16(p, 7));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_full_sync_sends_all_segments_and_deletes_after_ack);
    RUN_TEST(test_packets_respect_mtu);
    RUN_TEST(test_buffer_full_retries_without_gaps);
    RUN_TEST(test_disconnect_keeps_unacked_segment);
    RUN_TEST(test_ack_timeout_keeps_segment);
    RUN_TEST(test_wrong_ack_is_ignored);
    RUN_TEST(test_empty_log_sends_done_immediately);
    RUN_TEST(test_live_every_second_only_when_connected);
    RUN_TEST(test_set_time_writes_rtc_and_timezone);
    return UNITY_END();
}
