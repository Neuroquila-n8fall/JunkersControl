#include <Arduino.h>
#include <can_processor.h>
#include <heating.h>
#include <main.h>
#include <ACAN2515.h>
#include <mqtt.h>
#include <telnet.h>
#include <timesync.h>
#include <configuration.h>

//-- Preprocessor 
#define ST(A) #A
#define STR(A) ST(A)
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

ACAN2515 can(MCP2515_CS, SPI, MCP2515_INT);

double temp = 0.00F;

// Simply takes all current states into account and dispatches the setpoint message immediately.
void SetFeedTemperature(void *parameter)
{
  log_v("Begin Task");
  while (true)
  {

    CANMessage msg = PrepareMessage(configuration.CanAddresses.Heating.FeedSetpoint, 1);
    char printbuf[255];
    int feedSetpoint = 0;

    // Get raw Setpoint
    commandedValues.Heating.CalculatedFeedSetpoint = CalculateFeedTemperature();

    // Transform it into the int representation
    feedSetpoint = ConvertFeedTemperature(commandedValues.Heating.CalculatedFeedSetpoint);

    if (Debug)
    {
      log_i("Feed Setpoint is %.2f, INT representation (half steps) is %i", commandedValues.Heating.CalculatedFeedSetpoint, feedSetpoint);
    }

    msg.data[0] = feedSetpoint;
    // Wait before sending anything
    while (!SafeToSendMessage)
      vTaskDelay(100 / portTICK_PERIOD_MS);
    SendMessage(msg);

    vTaskDelay(30000 / portTICK_PERIOD_MS);
  }
}

void setupCan()
{
  log_v("Setting up Can Interface");
  SPI.begin(MCP2515_SCK, MCP2515_MISO, MCP2515_MOSI);
  uint32_t frequency = configuration.CanModuleConfig.CAN_Quartz * 1000UL * 1000UL;
  ACAN2515Settings settings(frequency, 10UL * 1000UL); // CAN bit rate 10 kb/s
  const uint16_t errorCode = can.begin(settings, []
                                       { can.isr(); });
  if (errorCode == 0 && Debug)
  {
    log_i("Bit Rate prescaler: %i", settings.mBitRatePrescaler);
    log_i("Propagation Segment: %i", settings.mPropagationSegment);
    log_i("Phase segment 1: %i", settings.mPhaseSegment1);
    log_i("Phase segment 2: %i", settings.mPhaseSegment2);
    log_i("SJW: %i", settings.mSJW);
    log_i("Triple Sampling: %s", settings.mTripleSampling ? "yes" : "no");
    log_i("Actual bit rate: %ibit/s", settings.actualBitRate());
    log_i("Exact bit rate ? %s", settings.exactBitRate() ? "yes" : "no");
    log_i("Sample point: %i%", settings.samplePointFromBitStart());
  }
  if (errorCode != 0)
  {
    log_e("Configuration error 0x%02x", errorCode);
  }
}

// Process incoming CAN messages
void processCan()
{
  CANMessage Message;
  if (can.receive(Message))
  {
    unsigned long curMillis = millis();

    if (Debug || configuration.General.Sniffing)
    {
      WriteMessage(Message);
    }

    // Check for other controllers on the network by watching out for messages that are greater than 0x250
    if (Message.id > 0x250 && Message.id < 0x260)
    {
      controllerMessageTimer = curMillis;

      // Bail out if we're already disabled.
      if (!Override)
        return;

      // Switch off override if another controller sends messages on the network.
      Override = false;

      log_i("[%s]\t Detected another controller on the network. Disabling Override", myTZ.dateTime("d-M-y H:i:s.v").c_str());
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

    unsigned int rawTemp = 0;
    char errorCode[2];

    // Take note of the last time we received a message from the boiler
    if (Message.id < 0x250 || Message.id > 0x260)
    {
      lastHeatingMessageTime = millis();
    }

    //[HC] - [Controller] - Max. possible feed temperature
    // Data Type: INT
    // Value: Data / 2.0
    if (Message.id == configuration.CanAddresses.Heating.FeedMax)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Heating.FeedMaximum = temp;
    }

    //[HC] - [Controller] - Current feed temperature
    // Data Type: INT
    // Value: Data / 2.0
    if (Message.id == configuration.CanAddresses.Heating.FeedCurrent)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Heating.FeedCurrent = temp;
    }

    //[DHW] - [Controller] - Max. possible water temperature -or- target temperature when running in heating battery mode
    // Data Type: INT
    // Value: Data / 2.0
    if (Message.id == configuration.CanAddresses.HotWater.MaxTemperature)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Heating.BufferWaterTemperatureMaximum = temp;
    }

    //[DHW] - [Controller] - Current water temperature
    // Data Type: INT
    // Value: Data / 2.0
    if (Message.id == configuration.CanAddresses.HotWater.CurrentTemperature)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Heating.BufferWaterTemperatureCurrent = temp;
    }

    //[DHW] - [Controller] - Max. water temperature (limited by boiler dial setting)
    // Data Type: INT
    // Value: Data / 2.0
    if (Message.id == configuration.CanAddresses.HotWater.MaxTemperature)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Hotwater.MaximumTemperature = temp;
    }

    //[DHW] - [Controller] - Current water feed or battery temperature
    // Data Type: INT
    // Value: Data / 2.0
    if (Message.id == configuration.CanAddresses.HotWater.CurrentTemperature)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Hotwater.TemperatureCurrent = temp;
    }

    //[Controller] - Error Byte
    // Data Type: Byte
    // Value: 0x00 = Operational
    //       Error codes and their meaning vary between models. See your manual for details.
    if (Message.id == configuration.CanAddresses.General.Error)
    {
      ceraValues.General.Error = Message.data[0];
    }

    //[Controller] - Current outside temperature
    // Data Type: Byte Concat
    // Value: (Data[0] & Data[1]) / 100.0
    if (Message.id == configuration.CanAddresses.Heating.OutsideTemperature)
    {
      // Concat bytes 0 and 1 and divide the resulting INT by 100
      rawTemp = (Message.data[0] << 8) + Message.data[1];
      temp = rawTemp / 100.0;
      
      // Temperature Delta is too high
      if(abs(ceraValues.General.OutsideTemperature - temp) > 30 && ceraValues.General.HasReceivedOT)
      {
        log_w("[%s]\t Detected a massive spike in Outside Temperature readings. Was %.2f is now %.2f. Check if the cabling is correct of both CAN Bus and Temperature Sensor!", myTZ.dateTime("d-M-y H:i:s.v").c_str(), ceraValues.General.OutsideTemperature, temp);
        return;
      }

      // Temperatures above 200 are considered invalid.
      if (temp > 200.0)
      {
        log_w("[%s]\t Received invalid outside temperature reading. Check if the Sensor is connected properly and isn't faulty.", myTZ.dateTime("d-M-y H:i:s.v").c_str());
        return;
      };

      if (!ceraValues.General.HasReceivedOT)
      {
        ceraValues.General.HasReceivedOT = true;
      }

      ceraValues.General.OutsideTemperature = temp;
    }

    //[Controller] - Gas Burner Flame Status
    // Data Type: Bit
    // Value: 1 = On | 0 = Off
    if (Message.id == configuration.CanAddresses.General.FlameLit)
    {
      ceraValues.General.FlameLit = Message.data[0];
    }

    //[HC] - [Controller] - HC Pump Operation
    // Data Type: Bit
    // Value: 1 = On | 0 = Off
    if (Message.id == configuration.CanAddresses.Heating.Pump)
    {
      ceraValues.Heating.PumpActive = Message.data[0];
    }

    //[DHW] - [Controller] - Hot Water Buffer Operation
    // Data Type: Bit
    // Value: 1 = Enabled | 0 = Disabled
    if (Message.id == configuration.CanAddresses.HotWater.BufferOperation)
    {
      ceraValues.Hotwater.BufferMode = Message.data[0];
    }

    //[HC] - [Controller] - Current Seasonal Operation Mode (Set by Dial on the boiler panel)
    // Data Type: Bit
    // Value: 1 = Winter | 0 = Summer
    if (Message.id == configuration.CanAddresses.Heating.Season)
    {
      ceraValues.Heating.Season = Message.data[0];
    }

    //[HC] - [RC] - Heating Operating
    // Data Type: Bit
    // Value: 1 = On | 0 = Off
    if (Message.id == configuration.CanAddresses.Heating.Operation)
    {
      ceraValues.Heating.Active = Message.data[0];
    }

    //[HC] - [RC] - Heating Power
    // Data Type: INT
    // Value: 0-255 = 0-100%
    if (Message.id == configuration.CanAddresses.Heating.Power)
    {
      ceraValues.Heating.HeatingPower = Message.data[0];
    }

    //[HC] - [RC] - Setpoint Feed Temperature
    // Data Type: INT
    // Value: Value / 2.0
    // Set: Value as half-centigrade steps i.e. 35.5
    if (Message.id == configuration.CanAddresses.Heating.FeedSetpoint)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Heating.FeedSetpoint = temp;
    }

    //[DHW] - [RC] - Setpoint water temperature
    // Data Type: INT
    // Value: Data / 2.0
    // Set: Value as half-centigrade steps i.e. 45.5
    if (Message.id == configuration.CanAddresses.HotWater.SetpointTemperature)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Hotwater.SetPoint = temp;
    }

    //[DHW] - [RC] - "Hot Water Now" (Warmwasser SOFORT in German)
    // Data Type: Bit
    // Value: 1 = Enabled | 0 = Disabled
    if (Message.id == configuration.CanAddresses.HotWater.Now)
    {
      ceraValues.Hotwater.Now = Message.data[0];
    }

    //[RC] - Date and Time
    // Data Type: Multibyte
    // Value: Data[0] = Day Of Week Number ( 1 = Monday)
    //       Data[1] = Hours (0-23)
    //       Data[2] = Minutes (0-59)
    //       Data[3] = Always '4' - Unknown meaning.
    if (Message.id == configuration.CanAddresses.General.DateTime)
    {
      ceraValues.Time.DayOfWeek = Message.data[0];
      ceraValues.Time.Hours = Message.data[1];
      ceraValues.Time.Minutes = Message.data[2];
    }

    //[DHW] - [RC] - Setpoint water temperature (Continuous-Flow Mode)
    // Data Type: INT
    // Value: Data / 2.0
    if (Message.id == configuration
                          .CanAddresses
                          .HotWater
                          .ContinousFlowSetpointTemperature)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.Hotwater.ContinousFlowSetpoint = temp;
    }

    //[MC] - [Controller] - Mixed-Circuit Pump Operation
    // Data Type: Bit
    // Value: 1 = On | 0 = Off
    if (Message.id == configuration.CanAddresses.MixedCircuit.Pump)
    {
      ceraValues.MixedCircuit.PumpActive = Message.data[0];
    }

    //[MC] - [RC] - Setpoint Mixed-Circuit Feed Temperature
    // Data Type: INT
    // Value: Data / 2.0
    if (Message.id == configuration.CanAddresses.MixedCircuit.FeedSetpoint)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.MixedCircuit.FeedSetpoint = temp;
    }

    //[MC] - [RC] - Mixed-Circuit Economy Setting
    // Data Type: Bit
    // Value: 1 = On | 0 = Off
    if (Message.id == configuration.CanAddresses.MixedCircuit.Economy)
    {
      ceraValues.MixedCircuit.Economy = Message.data[0];
    }

    //[MC] - [Controller] - Mixed-Circuit Current Feed Temperature
    // Data Type: INT
    // Value: Data / 2.0
    if (Message.id == configuration.CanAddresses.MixedCircuit.FeedCurrent)
    {
      temp = Message.data[0] / 2.0;
      ceraValues.MixedCircuit.FeedCurrent = temp;
    }
  }
}
