#include <Arduino.h>
#include <can_processor.h>
#include <heating.h>
#include <main.h>
#include <ACAN2515.h>
#include <mqtt.h>
#include <logger.h>
#include <timesync.h>
#include <configuration.h>
#include <failsafe.h>
#include <cmath>
#include <feed_temperature.h>


//-- Set CS Pin via Build Flag
#ifndef MCP2515_CS
#define MCP2515_CS 5
#endif
//-- Set INT Pin via Build Flag
#ifndef MCP2515_INT
#define MCP2515_INT 17
#endif


static const byte MCP2515_SCK = 18;  // SCK input of MCP2515
static const byte MCP2515_MOSI = 23; // SDI input of MCP2515
static const byte MCP2515_MISO = 19; // SDO output of MCP2515

volatile uint16_t CanConfigErrorCode;

ACAN2515 can(MCP2515_CS, SPI, MCP2515_INT);

double temp = 0.00F;

// Fail-safe limits may lower a requested setpoint, but cannot defeat off or
// increase it beyond the observed physical boiler maximum.
static double limitEffectiveFeedTemperature(double requested)
{
  if (!ceraValues.Heating.HasReceivedFeedMaximum || requested <= 10.0)
    return 10.0;
  double maximum = ceraValues.Heating.FeedMaximum;
  double minimum = 10.0;
  if (IsFailSafeActive())
  {
    if (!std::isfinite(configuration.FailSafe.MaximumFeedTemperature))
      return 10.0;
    if (configuration.FailSafe.MaximumFeedTemperature < maximum)
      maximum = configuration.FailSafe.MaximumFeedTemperature;
    minimum = configuration.FailSafe.MinimumFeedTemperature;
  }
  return LimitFeedTemperatureOutput(ApplyFailSafeFeedLimits(requested), maximum, minimum);
}

// Simply takes all current states into account and dispatches the setpoint message immediately.
void SetFeedTemperature()
{
  CANMessage msg = PrepareMessage(configuration.CanAddresses.Heating.FeedSetpoint,1);
  int feedSetpoint;

  // Get raw Setpoint
  double effectiveSetpoint = limitEffectiveFeedTemperature(CalculateFeedTemperature());
  if (EnforceHeatingSetpointShutdown(effectiveSetpoint))
    effectiveSetpoint = limitEffectiveFeedTemperature(CalculateFeedTemperature());
  commandedValues.Heating.CalculatedFeedSetpoint = effectiveSetpoint;

  // Transform it into the int representation
  feedSetpoint = ConvertFeedTemperature(commandedValues.Heating.CalculatedFeedSetpoint);

  LogDebug("CAN: feed setpoint %.2f °C (raw %i half steps)", commandedValues.Heating.CalculatedFeedSetpoint, feedSetpoint);

  msg.data[0] = feedSetpoint;

  SendMessage(msg);
}

void setupCan()
{
  SPI.begin(MCP2515_SCK, MCP2515_MISO, MCP2515_MOSI);
  uint32_t frequency = configuration.CanModuleConfig.CAN_Quartz * 1000UL * 1000UL; // 16 MHz
  ACAN2515Settings settings(frequency, 10UL * 1000UL); // CAN bit rate 10 kb/s

  CanConfigErrorCode = can.begin(settings, []
                                       { can.isr(); });
  if (CanConfigErrorCode == 0)
  {
    LogDebug("CAN: %lu bit/s (%s), prescaler %u, prop %u, phase %u/%u, SJW %u, triple sampling %s, sample point %u%%",
             static_cast<unsigned long>(settings.actualBitRate()), settings.exactBitRate() ? "exact" : "approx",
             static_cast<unsigned>(settings.mBitRatePrescaler), static_cast<unsigned>(settings.mPropagationSegment),
             static_cast<unsigned>(settings.mPhaseSegment1), static_cast<unsigned>(settings.mPhaseSegment2),
             static_cast<unsigned>(settings.mSJW), settings.mTripleSampling ? "yes" : "no",
             static_cast<unsigned>(settings.samplePointFromBitStart()));
  }
  else
  {
    LogError("CAN: controller configuration error 0x%X", CanConfigErrorCode);
  }
}

// The boiler repeats burner, heating pump and season frames only while a
// controller keeps sending the zero-length info request 0x0F9 (issue #60).
// Without it the last value would otherwise be displayed as current forever.
static const unsigned long StatusValueTimeoutMs = 90UL * 1000UL;

enum StatusValue : uint8_t
{
  StatusFlame,
  StatusHeatingPump,
  StatusHeatingSeason,
  StatusValueCount
};

static const char *const StatusValueName[StatusValueCount] = {"burner flame", "heating pump", "heating season"};
static unsigned long statusValueLastFrame[StatusValueCount];

static bool &statusValueKnown(StatusValue value)
{
  switch (value)
  {
  case StatusFlame:
    return ceraValues.General.FlameLitKnown;
  case StatusHeatingPump:
    return ceraValues.Heating.PumpActiveKnown;
  default:
    return ceraValues.Heating.SeasonKnown;
  }
}

static void noteStatusValueFrame(StatusValue value, unsigned long now)
{
  statusValueLastFrame[value] = now;
  bool &known = statusValueKnown(value);
  if (!known)
  {
    known = true;
    LogInfo("CAN: %s status received, value known", StatusValueName[value]);
  }
}

static void expireStatusValues()
{
  const unsigned long now = millis();
  for (uint8_t i = 0; i < StatusValueCount; i++)
  {
    bool &known = statusValueKnown(static_cast<StatusValue>(i));
    if (known && now - statusValueLastFrame[i] >= StatusValueTimeoutMs)
    {
      known = false;
      LogInfo("CAN: no %s status for %lu s, value unknown. The boiler only sends it while a controller sends the 0x0F9 info request",
              StatusValueName[i], StatusValueTimeoutMs / 1000UL);
    }
  }
}

// Compares the received room controller clock with the NTP clock. Logs only when
// the verdict changes and never transmits a correction.
static void CheckControllerClock()
{
  static const char *const days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
  constexpr int minutesPerDay = 24 * 60;
  constexpr int minutesPerWeek = 7 * minutesPerDay;
  constexpr int toleranceMinutes = 5; // frames have minute resolution; allow clock skew

  int actual;
  if (!LocalMinuteOfWeek(actual))
    return;

  const int controller = (ceraValues.Time.DayOfWeek - 1) * minutesPerDay + ceraValues.Time.Hours * 60 + ceraValues.Time.Minutes;
  // Wrap to ±half a week so Sunday 23:59 vs Monday 00:01 is 2 minutes, not almost a week.
  const int difference = (controller - actual + minutesPerWeek + minutesPerWeek / 2) % minutesPerWeek - minutesPerWeek / 2;
  const bool off = abs(difference) > toleranceMinutes;
  const bool wasOff = ceraValues.Time.HasClockComparison && ceraValues.Time.ControllerClockOff;

  ceraValues.Time.HasClockComparison = true;
  ceraValues.Time.ControllerClockOff = off;
  ceraValues.Time.ControllerClockOffsetMinutes = difference;

  if (off && !wasOff)
    LogWarn("CAN: room controller clock %s %02d:%02d, actual %s %02d:%02d (off by %+d min)",
            days[ceraValues.Time.DayOfWeek - 1], ceraValues.Time.Hours, ceraValues.Time.Minutes,
            days[actual / minutesPerDay], actual % minutesPerDay / 60, actual % 60, difference);
  else if (!off && wasOff)
    LogInfo("CAN: room controller clock matches the actual time again");
}

// Process incoming CAN messages
void processCan()
{
  CANMessage Message;
  // Drain the receive queue on every pass. Network maintenance must never
  // leave old boiler states sitting behind a single-frame-per-loop bottleneck.
  while (can.receive(Message))
  {
    unsigned long curMillis = millis();

    // Always publish the compact event for the web analyzer. WriteMessage
    // keeps verbose console output behind Debug/Sniffing.
    WriteMessage(Message);

    // Controller-frame ranges differ between BM1 installations. Keep the
    // range configurable and inclusive so the standard 0x250 operation frame
    // also suppresses takeover.
    if (Message.id >= configuration.CanModuleConfig.ControllerAddressMin &&
        Message.id <= configuration.CanModuleConfig.ControllerAddressMax)
    {
      controllerMessageTimer = curMillis;

      if (OverrideControl)
      {
        // Switch off override if another controller sends messages on the network.
        OverrideControl = false;
        LogInfo("CAN: other controller detected, override disabled");
      }
    }

    /*************************************
     * Terms
     * Controller = Built-in controller of your boiler
     * RC = The "Room Controller" or "Remote Control" or as I call it the "Consumer Controller" where you usually set up time schedules and temperatures.
     * HC = Heating Circuit
     * DHW = Domestic Hot Water
     * MC = Mixed Circuit
     * FT = Feed Temperature
     * Basepoint = Outside temperature at which the heating should deliver the highest possible feed temperature.
     * Endpoint = Outside temperature at which the heating should deliver the lowest possible feed temperature. Also known as "cut-off" temperature (depends on who you are talking with about this topic ;))
     **************************************/

    
    // Mixed-circuit frames are "<circuit> <value>"; the value is in data[1]. 1-byte frames keep it in data[0].
    const bool dataFrame = !Message.ext && !Message.rtr;
    const uint8_t mixedCircuitValueIndex = Message.len >= 2 ? 1 : 0;

    // Take note of the last time we received a message from the boiler
    if (Message.id < configuration.CanModuleConfig.ControllerAddressMin ||
        Message.id > configuration.CanModuleConfig.ControllerAddressMax)
    {
      lastHeatingMessageTime = millis();
    }

    //[HC] - [Controller] - Max. possible feed temperature
    // Data Type: INT
    // Value: Data / 2.0
    if (configuration.CanModuleConfig.Profiles.Heating && Message.id == configuration.CanAddresses.Heating.FeedMax &&
        dataFrame && Message.len >= 1)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Heating.FeedMaximum = temp;
      ceraValues.Heating.HasReceivedFeedMaximum = true;
    }

    //[HC] - [Controller] - Current feed temperature
    // Data Type: INT
    // Value: Data / 2.0
    if (configuration.CanModuleConfig.Profiles.Heating && Message.id == configuration.CanAddresses.Heating.FeedCurrent &&
        dataFrame && Message.len >= 1)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Heating.FeedCurrent = temp;
    }

    //[DHW] - [Controller] - Max. possible water temperature -or- target temperature when running in heating battery mode
    // Data Type: INT
    // Value: Data / 2.0
    if (configuration.CanModuleConfig.Profiles.DomesticHotWater && Message.id == configuration.CanAddresses.HotWater.MaxTemperature &&
        dataFrame && Message.len >= 1)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Heating.BufferWaterTemperatureMaximum = temp;
    }

    //[DHW] - [Controller] - Current water temperature
    // Data Type: INT
    // Value: Data / 2.0
    if (configuration.CanModuleConfig.Profiles.DomesticHotWater && Message.id == configuration.CanAddresses.HotWater.CurrentTemperature &&
        dataFrame && Message.len >= 1)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Heating.BufferWaterTemperatureCurrent = temp;
    }

    //[DHW] - [Controller] - Max. water temperature (limited by boiler dial setting)
    // Data Type: INT
    // Value: Data / 2.0
    if (configuration.CanModuleConfig.Profiles.DomesticHotWater && Message.id == configuration.CanAddresses.HotWater.MaxTemperature &&
        dataFrame && Message.len >= 1)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Hotwater.MaximumTemperature = temp;
      // Dial positions seen on a boiler: holiday (umbrella) 0x14 = 10 °C, maximum ("E") 0x78 = 60 °C.
      ceraValues.Hotwater.DialHoliday = temp > 0.0 && temp <= 10.0;
      ceraValues.Hotwater.HasReceivedDialHoliday = true;
    }

    //[DHW] - [Controller] - Current water feed or battery temperature
    // Data Type: INT
    // Value: Data / 2.0
    if (configuration.CanModuleConfig.Profiles.DomesticHotWater && Message.id == configuration.CanAddresses.HotWater.CurrentTemperature &&
        dataFrame && Message.len >= 1)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Hotwater.TemperatureCurrent = temp;
    }

    //[Controller] - Error Byte
    // Data Type: Byte
    // Value: 0x00 = Operational
    //       Error codes and their meaning vary between models. See your manual for details.
    if (Message.id == configuration.CanAddresses.General.Error && dataFrame && Message.len >= 1)
    {
      ceraValues.General.Error = Message.data[0];
    }

    //[Controller] - Current outside temperature
    // Data Type: Byte Concat
    // Value: (Data[0] & Data[1]) / 100.0
    if (configuration.CanModuleConfig.Profiles.Heating && Message.id == configuration.CanAddresses.Heating.OutsideTemperature &&
        dataFrame && Message.len >= 2)
    {
      // Concat payload and divide the resulting INT by 100
      short rawTemp = (Message.data[0] << 8) + Message.data[1];  
      temp = rawTemp / 100.0;
      
      // Temperature Delta is too high. Skip only this frame; the rest of the receive queue still needs draining.
      if(abs(ceraValues.General.OutsideTemperature - temp) > 30 && ceraValues.General.HasReceivedOT)
      {
        LogWarn("CAN: outside temperature spike %.2f → %.2f °C ignored, check CAN and sensor cabling", ceraValues.General.OutsideTemperature, temp);
        continue;
      }

      // Temperatures above 200 are considered invalid.
      if (temp > 200.0)
      {
        LogWarn("CAN: invalid outside temperature %.2f °C ignored, check sensor connection", temp);
        continue;
      }

      if (!ceraValues.General.HasReceivedOT)
      {
        ceraValues.General.HasReceivedOT = true;
      }

      ceraValues.General.OutsideTemperature = temp;
    }

    //[Controller] - Gas Burner Flame Status
    // Data Type: Bit
    // Value: 1 = On | 0 = Off
    if (Message.id == configuration.CanAddresses.General.FlameLit && dataFrame && Message.len >= 1)
    {
      ceraValues.General.FlameLit = Message.data[0];
      noteStatusValueFrame(StatusFlame, curMillis);
    }

    //[HC] - [Controller] - HC Pump Operation
    // Data Type: Bit
    // Value: 1 = On | 0 = Off
    if (configuration.CanModuleConfig.Profiles.Heating && Message.id == configuration.CanAddresses.Heating.Pump &&
        dataFrame && Message.len >= 1)
    {
      ceraValues.Heating.PumpActive = Message.data[0];
      noteStatusValueFrame(StatusHeatingPump, curMillis);
    }

    //[DHW] - [Controller] - Hot Water Buffer Operation
    // Data Type: Bit
    // Value: 1 = Enabled | 0 = Disabled
    if (configuration.CanModuleConfig.Profiles.DomesticHotWater && Message.id == configuration.CanAddresses.HotWater.BufferOperation &&
        dataFrame && Message.len >= 1)
    {
      ceraValues.Hotwater.BufferMode = Message.data[0];
    }

    //[HC] - [Controller] - Current Seasonal Operation Mode (Set by Dial on the boiler panel)
    // Data Type: Bit
    // Value: 1 = Winter | 0 = Summer
    if (configuration.CanModuleConfig.Profiles.Heating && Message.id == configuration.CanAddresses.Heating.Season &&
        dataFrame && Message.len >= 1)
    {
      ceraValues.Heating.Season = Message.data[0];
      noteStatusValueFrame(StatusHeatingSeason, curMillis);
    }

    //[HC] - [RC] - Economy (night) state, sent by the TA270 about every 30 seconds
    // Data Type: Bit
    // Value: 1 = Economy/Night | 0 = Normal/Day
    // Cerasmarter's own 0x253 request is never received back: the MCP2515 does not receive its own frames.
    if (configuration.CanModuleConfig.Profiles.Heating && Message.id == configuration.CanAddresses.Heating.Economy &&
        dataFrame && Message.len >= 1)
    {
      ceraValues.Heating.Economy = Message.data[0];
      ceraValues.Heating.HasReceivedEconomy = true;
    }

    //[HC] - [RC] - Heating Operating
    // Data Type: Bit
    // Value: 1 = On | 0 = Off
    if (configuration.CanModuleConfig.Profiles.Heating && Message.id == configuration.CanAddresses.Heating.Operation &&
        dataFrame && Message.len >= 1)
    {
      ceraValues.Heating.Active = Message.data[0];
    }

    //[HC] - [RC] - Heating Power
    // Data Type: INT
    // Value: 0-255 = 0-100%
    if (configuration.CanModuleConfig.Profiles.Heating && Message.id == configuration.CanAddresses.Heating.Power &&
        dataFrame && Message.len >= 1)
    {
      ceraValues.Heating.HeatingPower = Message.data[0];
    }

    //[HC] - [RC] - Setpoint Feed Temperature
    // Data Type: INT
    // Value: Value / 2.0
    // Set: Value as half-centigrade steps i.e. 35.5
    if (configuration.CanModuleConfig.Profiles.Heating && Message.id == configuration.CanAddresses.Heating.FeedSetpoint &&
        dataFrame && Message.len >= 1)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Heating.FeedSetpoint = temp;
      ceraValues.Heating.HasReceivedFeedSetpoint = true;
    }

    //[DHW] - [RC] - Setpoint water temperature
    // Data Type: INT
    // Value: Data / 2.0
    // Set: Value as half-centigrade steps i.e. 45.5
    if (configuration.CanModuleConfig.Profiles.DomesticHotWater && Message.id == configuration.CanAddresses.HotWater.SetpointTemperature &&
        dataFrame && Message.len >= 1)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Hotwater.SetPoint = temp;
      ceraValues.Hotwater.HasReceivedSetPoint = true;
    }

    //[DHW] - [RC] - "Hot Water Now" (Warmwasser SOFORT in German)
    // Data Type: Bit
    // Value: 1 = Enabled | 0 = Disabled
    if (configuration.CanModuleConfig.Profiles.DomesticHotWater && Message.id == configuration.CanAddresses.HotWater.Now &&
        dataFrame && Message.len >= 1)
    {
      ceraValues.Hotwater.Now = Message.data[0];
    }

    //[RC] - Date and Time
    // Data Type: Multibyte
    // Value: Data[0] = Day Of Week Number ( 1 = Monday)
    //       Data[1] = Hours (0-23)
    //       Data[2] = Minutes (0-59)
    //       Data[3] = Always '4' - Unknown meaning.
    if (Message.id == configuration.CanAddresses.General.DateTime && dataFrame && Message.len >= 3)
    {
      ceraValues.Time.DayOfWeek = Message.data[0];
      ceraValues.Time.Hours = Message.data[1];
      ceraValues.Time.Minutes = Message.data[2];
      ceraValues.Time.HasValidTime = Message.data[0] >= 1 && Message.data[0] <= 7 &&
                                     Message.data[1] <= 23 && Message.data[2] <= 59;
      if (ceraValues.Time.HasValidTime)
      {
        ceraValues.Time.LastUpdateMillis = millis();
        CheckControllerClock();
      }
    }

    //[DHW] - [RC] - Setpoint water temperature (Continuous-Flow Mode)
    // Data Type: INT
    // Value: Data / 2.0
    if (configuration.CanModuleConfig.Profiles.DomesticHotWater && Message.id == configuration
                          .CanAddresses
                          .HotWater
                          .ContinousFlowSetpointTemperature &&
        dataFrame && Message.len >= 1)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Hotwater.ContinousFlowSetpoint = temp;
      ceraValues.Hotwater.HasReceivedContinousFlowSetpoint = true;
    }

    //[MC] - [Controller] - Mixed-Circuit Pump Operation
    // Data Type: Bit
    // Value: 1 = On | 0 = Off
    // Payload: <circuit> <value>. 1-byte frames carry the value in data[0].
    if (configuration.CanModuleConfig.Profiles.MixedCircuit && Message.id == configuration.CanAddresses.MixedCircuit.Pump &&
        dataFrame && Message.len >= 1)
    {
      ceraValues.MixedCircuit.PumpActive = Message.data[mixedCircuitValueIndex];
    }

    //[MC] - [RC] - Setpoint Mixed-Circuit Feed Temperature
    // Data Type: INT
    // Value: Data / 2.0
    if (configuration.CanModuleConfig.Profiles.MixedCircuit && Message.id == configuration.CanAddresses.MixedCircuit.FeedSetpoint &&
        dataFrame && Message.len >= 1)
    {
      temp = Message.data[mixedCircuitValueIndex] / 2.0;
      ceraValues.MixedCircuit.FeedSetpoint = temp;
    }

    //[MC] - [RC] - Mixed-Circuit Economy Setting
    // Data Type: Bit
    // Value: 1 = On | 0 = Off
    if (configuration.CanModuleConfig.Profiles.MixedCircuit && Message.id == configuration.CanAddresses.MixedCircuit.Economy &&
        dataFrame && Message.len >= 1)
    {
      ceraValues.MixedCircuit.Economy = Message.data[mixedCircuitValueIndex];
    }

    //[MC] - [RC] - Mixed-Circuit Day Mode (Economy switch of the TA270)
    // Data Type: Bit
    // Value: 1 = Day/Normal | 0 = Economy (inverse of the Economy address above)
    // Observed on 0x415 as <circuit> <value>, repeated every 30 s (issue #61).
    if (configuration.CanModuleConfig.Profiles.MixedCircuit && Message.id == configuration.CanAddresses.MixedCircuit.DayMode &&
        dataFrame && Message.len >= 1)
    {
      ceraValues.MixedCircuit.Economy = Message.data[mixedCircuitValueIndex] == 0;
    }

    //[MC] - [Controller] - Mixed-Circuit Current Feed Temperature
    // Data Type: INT
    // Value: Data / 2.0
    if (configuration.CanModuleConfig.Profiles.MixedCircuit && Message.id == configuration.CanAddresses.MixedCircuit.FeedCurrent &&
        dataFrame && Message.len >= 1)
    {
      temp = Message.data[mixedCircuitValueIndex] / 2.0;
      ceraValues.MixedCircuit.FeedCurrent = temp;
    }
  }

  expireStatusValues();
}
