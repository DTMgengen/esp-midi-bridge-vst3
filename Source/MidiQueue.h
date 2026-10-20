#pragma once
#include <array>
#include <atomic>
#include <cstdint>
namespace esp {
struct DevicePacket {std::array<uint8_t,3> data{};uint32_t generation=0;};
// Exactly one audio producer and one device-thread consumer. No wake/event or
// system MIDI calls in push(). Overflow is reported, never overwritten.
class MidiQueue {
public:
    static constexpr uint32_t capacity=4096;
    bool push(const DevicePacket& packet) noexcept {
        const auto w=write.load(std::memory_order_relaxed);
        if(w-read.load(std::memory_order_acquire)>=capacity) return false;
        packets[w%capacity]=packet;write.store(w+1,std::memory_order_release);return true;
    }
    bool pop(DevicePacket& packet) noexcept {
        const auto r=read.load(std::memory_order_relaxed);
        if(r==write.load(std::memory_order_acquire)) return false;
        packet=packets[r%capacity];read.store(r+1,std::memory_order_release);return true;
    }
private:
    std::array<DevicePacket,capacity> packets{};
    std::atomic<uint32_t> read{0},write{0};
};
}
