#ifndef CAN_DECODER_TEST_CONTEXT_H
#define CAN_DECODER_TEST_CONTEXT_H

// PlatformIO force-includes this file in dependency translation units too.
// Only project translation units have the project's public headers available.
#if __has_include(<mqtt.h>)
#include <Arduino.h>
#include <ACAN2515.h>

// Replaces only the MCP2515 driver: the real src/can_processor.cpp drains
// receive() exactly as it does on the boiler bus. Tests fill it with push().
class TestCan
{
public:
    TestCan(uint8_t, SPIClass &, uint8_t) {}
    uint16_t begin(const ACAN2515Settings &, void (*)(void)) { return 0; }
    void isr() {}
    bool receive(CANMessage &frame)
    {
        if (head == tail)
            return false;
        frame = queue[head++ % capacity];
        return true;
    }
    bool push(const CANMessage &frame)
    {
        if (tail - head >= capacity)
            return false;
        queue[tail++ % capacity] = frame;
        return true;
    }

private:
    static constexpr uint8_t capacity = 8;
    CANMessage queue[capacity];
    uint32_t head = 0;
    uint32_t tail = 0;
};
#define ACAN2515 TestCan

// Controllable clock: project translation units read this instead of the
// hardware millis(). Tests move time with TestClock::Now() += ms. Arduino.h is
// included above so only later uses, not its declaration, are redirected.
namespace TestClock
{
inline unsigned long &Now()
{
    static unsigned long now = 0;
    return now;
}
} // namespace TestClock
#define millis() TestClock::Now()

// Production data types with main-loop and peripheral declarations replaced.
#include <mqtt.h>
#define _MAIN_H
#include <configuration.h>
#include <heating.h>
#include <failsafe.h>

extern bool OverrideControl;
extern unsigned long controllerMessageTimer;
extern unsigned long lastHeatingMessageTime;
extern void SendMessage(CANMessage msg);
extern CANMessage PrepareMessage(uint32_t id, int length = 8);
extern void WriteMessage(CANMessage msg, bool received = true);
#endif

#endif
