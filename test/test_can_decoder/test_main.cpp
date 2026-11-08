#include <Arduino.h>
#include <unity.h>
#include <can_processor.h>
#include <configuration.h>
#include <heating.h>
#include <mqtt.h>
#include <failsafe.h>
#include <math.h>
#include <string.h>

// The real src/can_processor.cpp decodes frames pushed into a stubbed receive
// queue (test/can_decoder_support/context.h). Mixed-circuit frames below are
// taken from the capture attached to issue #61 (TA270 + HMM, circuit 1).

namespace
{
void AssertNear(double actual, double expected)
{
    TEST_ASSERT_TRUE(fabs(actual - expected) <= 0.001);
}

CANMessage Frame(uint32_t id, uint8_t length, uint8_t b0 = 0, uint8_t b1 = 0, uint8_t b2 = 0)
{
    CANMessage frame;
    frame.id = id;
    frame.ext = false;
    frame.rtr = false;
    frame.len = length;
    frame.data[0] = b0;
    frame.data[1] = b1;
    frame.data[2] = b2;
    return frame;
}

void Push(const CANMessage &frame) { TEST_ASSERT_TRUE(can.push(frame)); }

// Pushes one frame and runs a drain of the receive queue.
void Process(const CANMessage &frame)
{
    Push(frame);
    processCan();
}

void ExpectQueueDrained()
{
    CANMessage leftover;
    TEST_ASSERT_FALSE(can.receive(leftover));
}

void TwoByteMixedCircuitFramesFromCaptureUseValueByte()
{
    Process(Frame(0x404, 2, 0x01, 0x01));
    TEST_ASSERT_TRUE(ceraValues.MixedCircuit.PumpActive);
    Process(Frame(0x404, 2, 0x01, 0x00));
    TEST_ASSERT_FALSE(ceraValues.MixedCircuit.PumpActive);

    Process(Frame(0x405, 2, 0x01, 0x55));
    AssertNear(ceraValues.MixedCircuit.FeedSetpoint, 42.5);
    Process(Frame(0x405, 2, 0x01, 0x54));
    AssertNear(ceraValues.MixedCircuit.FeedSetpoint, 42.0);

    Process(Frame(0x440, 2, 0x01, 0x43));
    AssertNear(ceraValues.MixedCircuit.FeedCurrent, 33.5);
    Process(Frame(0x440, 2, 0x01, 0x4A));
    AssertNear(ceraValues.MixedCircuit.FeedCurrent, 37.0);

    // Economy (0x407) does not occur in the capture; same <circuit> <value> layout.
    Process(Frame(0x407, 2, 0x01, 0x01));
    TEST_ASSERT_TRUE(ceraValues.MixedCircuit.Economy);
    Process(Frame(0x407, 2, 0x01, 0x00));
    TEST_ASSERT_FALSE(ceraValues.MixedCircuit.Economy);
}

void OneByteMixedCircuitFramesStillUseFirstByte()
{
    Process(Frame(0x404, 1, 0x01));
    TEST_ASSERT_TRUE(ceraValues.MixedCircuit.PumpActive);
    Process(Frame(0x405, 1, 0x55));
    AssertNear(ceraValues.MixedCircuit.FeedSetpoint, 42.5);
    Process(Frame(0x440, 1, 0x43));
    AssertNear(ceraValues.MixedCircuit.FeedCurrent, 33.5);
    Process(Frame(0x407, 1, 0x01));
    TEST_ASSERT_TRUE(ceraValues.MixedCircuit.Economy);
}

// TA270 economy switch (issue #61): 0x415 <circuit> <value>, 1 = day/normal, 0 = economy.
// Capture: 'Sparbetrieb ein' 16:39:53 -> 01 01 then 16:40:23 -> 01 00; 'aus' 16:40:40 -> 01 01 at 16:40:53.
void DayModeFramesFromCaptureSetInverseEconomy()
{
    Process(Frame(0x415, 2, 0x01, 0x01)); // 16:39:53, normal operation
    TEST_ASSERT_FALSE(ceraValues.MixedCircuit.Economy);
    Process(Frame(0x415, 2, 0x01, 0x00)); // 16:40:23, economy switched on
    TEST_ASSERT_TRUE(ceraValues.MixedCircuit.Economy);
    Process(Frame(0x415, 2, 0x01, 0x00)); // periodic repeat keeps economy
    TEST_ASSERT_TRUE(ceraValues.MixedCircuit.Economy);
    Process(Frame(0x415, 2, 0x01, 0x01)); // 16:40:53, economy switched off
    TEST_ASSERT_FALSE(ceraValues.MixedCircuit.Economy);
    Process(Frame(0x415, 2, 0x01, 0x01)); // 16:41:23 and later: unchanged
    TEST_ASSERT_FALSE(ceraValues.MixedCircuit.Economy);
}

void DayModeOneByteFrameUsesFirstByte()
{
    Process(Frame(0x415, 1, 0x00));
    TEST_ASSERT_TRUE(ceraValues.MixedCircuit.Economy);
    Process(Frame(0x415, 1, 0x01));
    TEST_ASSERT_FALSE(ceraValues.MixedCircuit.Economy);
}

void EconomyAddressKeepsOriginalMeaning()
{
    Process(Frame(0x407, 2, 0x01, 0x01));
    TEST_ASSERT_TRUE(ceraValues.MixedCircuit.Economy);
    Process(Frame(0x407, 2, 0x01, 0x00));
    TEST_ASSERT_FALSE(ceraValues.MixedCircuit.Economy);
    Process(Frame(0x407, 1, 0x01));
    TEST_ASSERT_TRUE(ceraValues.MixedCircuit.Economy);

    // The two sources feed the same field; each frame is decoded by its own address.
    Process(Frame(0x415, 2, 0x01, 0x01));
    TEST_ASSERT_FALSE(ceraValues.MixedCircuit.Economy);
    Process(Frame(0x407, 2, 0x01, 0x01));
    TEST_ASSERT_TRUE(ceraValues.MixedCircuit.Economy);
}

void DayModeIgnoredWhenProfileDisabledOrFrameMalformed()
{
    ceraValues.MixedCircuit.Economy = true;
    CANMessage remote = Frame(0x415, 2, 0x01, 0x01);
    remote.rtr = true;
    Process(remote);
    Process(Frame(0x415, 0, 0x01, 0x01)); // zero-length frame supplies no value byte
    CANMessage extended = Frame(0x415, 2, 0x01, 0x01);
    extended.ext = true;
    Process(extended);
    TEST_ASSERT_TRUE(ceraValues.MixedCircuit.Economy);

    configuration.CanModuleConfig.Profiles.MixedCircuit = false;
    Process(Frame(0x415, 2, 0x01, 0x01));
    TEST_ASSERT_TRUE(ceraValues.MixedCircuit.Economy);
}

// Boiler hot-water dial, reported on 0x204 (HotWater.MaxTemperature) as value/2 °C:
// holiday (umbrella) 0x14 = 10 °C, maximum ("E") 0x78 = 60 °C.
void DialHolidayIsUnknownUntilFirstFrame()
{
    configuration.CanModuleConfig.Profiles.DomesticHotWater = true;
    TEST_ASSERT_FALSE(ceraValues.Hotwater.HasReceivedDialHoliday);
    Process(Frame(0x207, 2, 0x03, 0xE8)); // unrelated traffic leaves it unknown
    TEST_ASSERT_FALSE(ceraValues.Hotwater.HasReceivedDialHoliday);

    Process(Frame(0x204, 1, 0x14));
    TEST_ASSERT_TRUE(ceraValues.Hotwater.HasReceivedDialHoliday);
    TEST_ASSERT_TRUE(ceraValues.Hotwater.DialHoliday);
    AssertNear(ceraValues.Hotwater.MaximumTemperature, 10.0);
}

void DialHolidayFollowsLastDialPosition()
{
    configuration.CanModuleConfig.Profiles.DomesticHotWater = true;
    Process(Frame(0x204, 1, 0x78));
    TEST_ASSERT_TRUE(ceraValues.Hotwater.HasReceivedDialHoliday);
    TEST_ASSERT_FALSE(ceraValues.Hotwater.DialHoliday);
    AssertNear(ceraValues.Hotwater.MaximumTemperature, 60.0);
    Process(Frame(0x204, 1, 0x14));
    TEST_ASSERT_TRUE(ceraValues.Hotwater.DialHoliday);
    Process(Frame(0x204, 1, 0x15)); // 10.5 °C is above the holiday position
    TEST_ASSERT_FALSE(ceraValues.Hotwater.DialHoliday);
    Process(Frame(0x204, 1, 0x14));
    TEST_ASSERT_TRUE(ceraValues.Hotwater.DialHoliday);
    Process(Frame(0x204, 1, 0x01)); // 0.5 °C: still the lowest range
    TEST_ASSERT_TRUE(ceraValues.Hotwater.DialHoliday);
    Process(Frame(0x204, 1, 0x00)); // 0 °C is not a dial position: not holiday
    TEST_ASSERT_FALSE(ceraValues.Hotwater.DialHoliday);
    TEST_ASSERT_TRUE(ceraValues.Hotwater.HasReceivedDialHoliday);
}

void DialHolidayIgnoresMalformedFramesAndDisabledProfile()
{
    configuration.CanModuleConfig.Profiles.DomesticHotWater = true;
    CANMessage remote = Frame(0x204, 1, 0x14);
    remote.rtr = true;
    Process(remote);
    CANMessage extended = Frame(0x204, 1, 0x14);
    extended.ext = true;
    Process(extended);
    Process(Frame(0x204, 0, 0x14)); // zero-length frame supplies no value byte
    TEST_ASSERT_FALSE(ceraValues.Hotwater.HasReceivedDialHoliday);

    Process(Frame(0x204, 1, 0x78));
    Process(remote);
    Process(extended);
    Process(Frame(0x204, 0, 0x14));
    TEST_ASSERT_TRUE(ceraValues.Hotwater.HasReceivedDialHoliday);
    TEST_ASSERT_FALSE(ceraValues.Hotwater.DialHoliday);

    ceraValues.Hotwater = CeraValues::HotWater_{};
    configuration.CanModuleConfig.Profiles.DomesticHotWater = false;
    Process(Frame(0x204, 1, 0x14));
    TEST_ASSERT_FALSE(ceraValues.Hotwater.HasReceivedDialHoliday);
}

void ShortRemoteAndExtendedFramesAreIgnored()
{
    ceraValues.MixedCircuit.FeedSetpoint = 20.0;
    ceraValues.Heating.FeedCurrent = 21.0;
    ceraValues.General.OutsideTemperature = 5.0;
    ceraValues.Time.HasValidTime = false;

    // Zero-length frames: fixed storage holds bytes the frame never supplied.
    Process(Frame(0x405, 0, 0x01, 0x55));
    Process(Frame(0x201, 0, 0x55));
    Process(Frame(0x207, 1, 0x03, 0xE8));
    Process(Frame(0x256, 2, 0x03, 0x0A, 0x1E));
    AssertNear(ceraValues.MixedCircuit.FeedSetpoint, 20.0);
    AssertNear(ceraValues.Heating.FeedCurrent, 21.0);
    AssertNear(ceraValues.General.OutsideTemperature, 5.0);
    TEST_ASSERT_FALSE(ceraValues.Time.HasValidTime);

    CANMessage remote = Frame(0x405, 2, 0x01, 0x55);
    remote.rtr = true;
    Process(remote);
    CANMessage extended = Frame(0x201, 2, 0x55, 0x55);
    extended.ext = true;
    Process(extended);
    AssertNear(ceraValues.MixedCircuit.FeedSetpoint, 20.0);
    AssertNear(ceraValues.Heating.FeedCurrent, 21.0);

    // The same frames are decoded once they are well-formed.
    Process(Frame(0x405, 2, 0x01, 0x55));
    Process(Frame(0x201, 1, 0x55));
    Process(Frame(0x207, 2, 0x03, 0xE8));
    Process(Frame(0x256, 3, 0x03, 0x0A, 0x1E));
    AssertNear(ceraValues.MixedCircuit.FeedSetpoint, 42.5);
    AssertNear(ceraValues.Heating.FeedCurrent, 42.5);
    AssertNear(ceraValues.General.OutsideTemperature, 10.0);
    TEST_ASSERT_TRUE(ceraValues.Time.HasValidTime);
    TEST_ASSERT_EQUAL_INT(10, ceraValues.Time.Hours);
    TEST_ASSERT_EQUAL_INT(30, ceraValues.Time.Minutes);
}

void OutsideTemperatureSpikeDoesNotAbortDrain()
{
    Process(Frame(0x207, 2, 0x03, 0xE8)); // 10.00 °C accepted
    TEST_ASSERT_TRUE(ceraValues.General.HasReceivedOT);

    // 45.00 °C is a >30 K jump: dropped, later frames of the same drain still decode.
    Push(Frame(0x207, 2, 0x11, 0x94));
    Push(Frame(0x405, 2, 0x01, 0x55));
    Push(Frame(0x440, 2, 0x01, 0x43));
    processCan();
    AssertNear(ceraValues.General.OutsideTemperature, 10.0);
    AssertNear(ceraValues.MixedCircuit.FeedSetpoint, 42.5);
    AssertNear(ceraValues.MixedCircuit.FeedCurrent, 33.5);
    ExpectQueueDrained();
}

void InvalidOutsideTemperatureDoesNotAbortDrain()
{
    // 250.00 °C is invalid; with no previous reading only the >200 check applies.
    Push(Frame(0x207, 2, 0x61, 0xA8));
    Push(Frame(0x405, 2, 0x01, 0x55));
    processCan();
    TEST_ASSERT_FALSE(ceraValues.General.HasReceivedOT);
    AssertNear(ceraValues.MixedCircuit.FeedSetpoint, 42.5);
    ExpectQueueDrained();
}

} // namespace

void setUp()
{
    ceraValues = CeraValues{};
    CANMessage stale;
    while (can.receive(stale))
    {
    }

    configuration.CanModuleConfig.Profiles.Heating = true;
    configuration.CanModuleConfig.Profiles.MixedCircuit = true;
    configuration.CanModuleConfig.Profiles.DomesticHotWater = false;
    configuration.CanModuleConfig.ControllerAddressMin = 0x250;
    configuration.CanModuleConfig.ControllerAddressMax = 0x25F;
    // Template defaults (assets/Templates/Configurations/configuration.json).
    configuration.CanAddresses.Heating.FeedCurrent = 0x201;
    configuration.CanAddresses.Heating.FeedMax = 0x200;
    configuration.CanAddresses.Heating.OutsideTemperature = 0x207;
    configuration.CanAddresses.General.DateTime = 0x256;
    configuration.CanAddresses.MixedCircuit.Pump = 0x404;
    configuration.CanAddresses.MixedCircuit.FeedSetpoint = 0x405;
    configuration.CanAddresses.MixedCircuit.FeedCurrent = 0x440;
    configuration.CanAddresses.MixedCircuit.Economy = 0x407;
    configuration.CanAddresses.HotWater.SetpointTemperature = 0x203;
    configuration.CanAddresses.HotWater.MaxTemperature = 0x204;
    configuration.CanAddresses.HotWater.CurrentTemperature = 0x205;
    configuration.CanAddresses.HotWater.Now = 0x254;
    configuration.CanAddresses.HotWater.BufferOperation = 0x20B;
    configuration.CanAddresses.HotWater.ContinousFlowSetpointTemperature = 0x255;
    configuration.CanAddresses.MixedCircuit.DayMode = 0x415;
    // Unrelated decoders must not match the test frames.
    configuration.CanAddresses.Heating.FeedSetpoint = 0x252;
    configuration.CanAddresses.Heating.Pump = 0x20A;
    configuration.CanAddresses.Heating.Season = 0x20C;
    configuration.CanAddresses.Heating.Operation = 0x250;
    configuration.CanAddresses.Heating.Power = 0x251;
    configuration.CanAddresses.General.FlameLit = 0x209;
    configuration.CanAddresses.General.Error = 0x206;
}

void tearDown() {}

bool OverrideControl = false;
unsigned long controllerMessageTimer = 0;
unsigned long lastHeatingMessageTime = 0;
CeraValues ceraValues;
Configuration configuration;
CommandedValues commandedValues;

void LogError(const char *, ...) {}
void LogWarn(const char *, ...) {}
void LogInfo(const char *, ...) {}
void LogDebug(const char *, ...) {}

void SendMessage(CANMessage) {}
CANMessage PrepareMessage(uint32_t id, int length)
{
    CANMessage message;
    message.id = id;
    message.len = length;
    return message;
}
void WriteMessage(CANMessage, bool) {}

// Referenced by SetFeedTemperature(), which these tests never call.
double CalculateFeedTemperature() { return 10.0; }
int ConvertFeedTemperature(double temperature) { return static_cast<int>(temperature * 2.0); }
bool EnforceHeatingSetpointShutdown(double) { return false; }
bool IsFailSafeActive() { return false; }
double ApplyFailSafeFeedLimits(double temperature) { return temperature; }

void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(TwoByteMixedCircuitFramesFromCaptureUseValueByte);
    RUN_TEST(DayModeFramesFromCaptureSetInverseEconomy);
    RUN_TEST(DayModeOneByteFrameUsesFirstByte);
    RUN_TEST(EconomyAddressKeepsOriginalMeaning);
    RUN_TEST(DayModeIgnoredWhenProfileDisabledOrFrameMalformed);
    RUN_TEST(DialHolidayIsUnknownUntilFirstFrame);
    RUN_TEST(DialHolidayFollowsLastDialPosition);
    RUN_TEST(DialHolidayIgnoresMalformedFramesAndDisabledProfile);
    RUN_TEST(OneByteMixedCircuitFramesStillUseFirstByte);
    RUN_TEST(ShortRemoteAndExtendedFramesAreIgnored);
    RUN_TEST(OutsideTemperatureSpikeDoesNotAbortDrain);
    RUN_TEST(InvalidOutsideTemperatureDoesNotAbortDrain);
    UNITY_END();
}

void loop() {}
