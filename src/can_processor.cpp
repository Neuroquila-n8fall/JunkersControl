#include <Arduino.h>
#include <can_processor.h>
#include <heating.h>
#include <main.h>
#include <ACAN2515.h>
#include <mqtt.h>
#include <telnet.h>
#include <timesync.h>

//——————————————————————————————————————————————————————————————————————————————
//  MCP2515 Driver object
//——————————————————————————————————————————————————————————————————————————————

ACAN2515 can(MCP2515_CS, SPI, MCP2515_INT);


//Simply takes all current states into account and dispatches the setpoint message immediately.
void SetFeedTemperature()
{
  CANMessage msg = PrepareMessage(0x252);  

  char printbuf[255];
  int feedSetpoint = 0;

  //Get raw Setpoint
  mqttCommandedFeedTemperature = CalculateFeedTemperature();
  //Transform it into the int representation
  feedSetpoint = ConvertFeedTemperature(mqttCommandedFeedTemperature);

  msg.data[0] = feedSetpoint;
  if (Debug)
  {
    sprintf(printbuf, "DEBUG SETFEEDTEMPERATURE: Feed Setpoint is %.2f, INT representation (half steps) is %i", mqttCommandedFeedTemperature, feedSetpoint);
    String message(printbuf);
    WriteToConsoles(message + "\r\n");
  }
}

void setupCan()
{

  //Setup CAN Module
  SPI.begin(MCP2515_SCK, MCP2515_MISO, MCP2515_MOSI);
  ACAN2515Settings settings(QUARTZ_FREQUENCY, 10UL * 1000UL); // CAN bit rate 10 kb/s

  const uint16_t errorCode = can.begin(settings, [] { can.isr(); });
  if (errorCode == 0)
  {
    Serial.print("Bit Rate prescaler: ");
    Serial.println(settings.mBitRatePrescaler);
    Serial.print("Propagation Segment: ");
    Serial.println(settings.mPropagationSegment);
    Serial.print("Phase segment 1: ");
    Serial.println(settings.mPhaseSegment1);
    Serial.print("Phase segment 2: ");
    Serial.println(settings.mPhaseSegment2);
    Serial.print("SJW: ");
    Serial.println(settings.mSJW);
    Serial.print("Triple Sampling: ");
    Serial.println(settings.mTripleSampling ? "yes" : "no");
    Serial.print("Actual bit rate: ");
    Serial.print(settings.actualBitRate());
    Serial.println(" bit/s");
    Serial.print("Exact bit rate ? ");
    Serial.println(settings.exactBitRate() ? "yes" : "no");
    Serial.print("Sample point: ");
    Serial.print(settings.samplePointFromBitStart());
    Serial.println("%");
  }
  else
  {
    Serial.print("Configuration error 0x");
    Serial.println(errorCode, HEX);
  }
}

//Process incoming CAN messages
void processCan()
{
  CANMessage Message;
  if (can.receive(Message))
  {
    unsigned long curMillis = millis();
    //Buffer for sending console output. 100 chars should be enough for now:
    //[25-Aug-18 14:32:53.282]\tCAN: [0000] Data: FF (255)\tFF (255)\tFF (255)\tFF (255)\tFF (255)
    char printBuf[100];
    //Buffer for storing the formatted values. We have to expect 'FF (255)' which is 8 bytes + 1 for string overhead \0
    char dataBuf[9];
    String data;

    for (int x = 0; x < Message.len; x++)
    {
      //A little bit of trickery to assemble the data bytes into a nicely formatted string
      sprintf(dataBuf, "%02X (%i)", Message.data[x], Message.data[x]);
      //Convert char array to string
      String temp(dataBuf);
      //Get rid of trailing spaces
      temp.trim();
      //Concat
      data += temp;
      //Add tab between data
      if (x < Message.len - 1)
      {
        data += "\t";
      }
    }

    //Check for other controllers on the network by watching out for messages that are greater than 0x250
    if (Message.id > 0x250)
    {
      //Switch off override if another controller sends messages on the network.
      Override = false;
      controllerMessageTimer = curMillis;
      WriteToConsoles("Detected another controller on the network. Disabling Override\r\n");
    }

    //Print string
    sprintf(printBuf, "CAN: [%04X] Data:\t", Message.id);
    String consoleMessage(printBuf);
    consoleMessage = myTZ.dateTime("[d-M-y H:i:s.v] - ") + consoleMessage;
    consoleMessage += data;
    WriteToConsoles(consoleMessage);
    WriteToConsoles("\r\n");

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


    //Take note of the last time we received a message from the boiler
    if (Message.id < 0x250)
    {
      lastHeatingMessageTime = millis();
    }
    

    switch (Message.id)
    {

    //[HC] - [Controller] - Max. possible feed temperature
    //Data Type: INT
    //Value: Data / 2.0
    case 0x200:
      temp = Message.data[0] / 2.0;
      hcMaxFeed = temp;
      break;

    //[HC] - [Controller] - Current feed temperature
    //Data Type: INT
    //Value: Data / 2.0
    case 0x201:
      temp = Message.data[0] / 2.0;
      hcCurrentFeed = temp;
      break;

    //[DHW] - [Controller] - Max. possible water temperature -or- target temperature when running in heating battery mode
    //Data Type: INT
    //Value: Data / 2.0
    case 0x202:
      temp = Message.data[0] / 2.0;
      hcMaxWaterTemperature = temp;
      break;

    //[DHW] - [Controller] - Current water temperature
    //Data Type: INT
    //Value: Data / 2.0
    case 0x203:
      temp = Message.data[0] / 2.0;
      hcCurrentWaterTemperature = temp;
      break;

    //[DHW] - [Controller] - Max. water temperature (limited by boiler dial setting)
    //Data Type: INT
    //Value: Data / 2.0
    case 0x204:
      temp = Message.data[0] / 2.0;
      break;

    //[DHW] - [Controller] - Current water feed or battery temperature
    //Data Type: INT
    //Value: Data / 2.0
    case 0x205:
      temp = Message.data[0] / 2.0;
      break;

    //[Controller] - Error Byte
    //Data Type: Byte
    //Value: 0x00 = Operational
    //       Error codes and their meaning vary between models. See your manual for details.
    case 0x206:
      status = Message.data[0];
      String(status).toCharArray(errorCode, 2);
      mqttErrorCode = status;
      break;

    //[Controller] - Current outside temperature
    //Data Type: Byte Concat
    //Value: (Data[0] & Data[1]) / 100.0
    case 0x207:
      //Concat bytes 0 and 1 and divide the resulting INT by 100
      rawTemp = (Message.data[0] << 8) + Message.data[1];
      temp = rawTemp / 100.0;
      //Temperatures above 200 are considered invalid.
      if(temp > 200.0) {
        WriteToConsoles("Received invalid outside temperature reading. Check if the Sensor is connected properly and isn't faulty.");
        break;
      };
      OutsideTemperatureSensor = temp;
      break;

    //Unknown
    case 0x208:
      break;

    //[Controller] - Gas Burner Flame Status
    //Data Type: Bit
    //Value: 1 = On | 0 = Off
    case 0x209:
      flame = Message.data[0];
      break;

    //[HC] - [Controller] - HC Pump Operation
    //Data Type: Bit
    //Value: 1 = On | 0 = Off
    case 0x20A:
      hcPump = Message.data[0];
      break;

    //[DHW] - [Controller] - Hot Water Battery Operation
    //Data Type: Bit
    //Value: 1 = Enabled | 0 = Disabled
    case 0x20B:
      hwBatteryMode = Message.data[0];
      break;

    //[HC] - [Controller] - Current Seasonal Operation Mode (Set by Dial on the boiler panel)
    //Data Type: Bit
    //Value: 1 = Winter | 0 = Summer
    case 0x20C:
      hcSeason = Message.data[0];
      break;

    //[HC] - [RC] - Heating Operating
    //Data Type: Bit
    //Value: 1 = On | 0 = Off
    case 0x250:
      hcActive = Message.data[0];
      break;

    //[HC] - [RC] - Heating Power
    //Data Type: INT
    //Value: 0-255 = 0-100%
    case 0x251:
      hcHeatingPower = Message.data[0];
      break;

    //[HC] - [RC] - Setpoint Feed Temperature
    //Data Type: INT
    //Value: Value / 2.0
    //Set: Value as half-centigrade steps i.e. 35.5
    case 0x252:
      temp = Message.data[0] / 2.0;
      break;

    //[DHW] - [RC] - Setpoint water temperature
    //Data Type: INT
    //Value: Data / 2.0
    //Set: //Set: Value as half-centigrade steps i.e. 45.5
    case 0x253:
      temp = Message.data[0] / 2.0;
      break;

    //[DHW] - [RC] - "Hot Water Now" (Warmwasser SOFORT in German)
    //Data Type: Bit
    //Value: 1 = Enabled | 0 = Disabled
    case 0x254:
      hwNow = Message.data[0];
      break;

    //[RC] - Date and Time
    //Data Type: Multibyte
    //Value: Data[0] = Day Of Week Number ( 1 = Monday)
    //       Data[1] = Hours (0-23)
    //       Data[2] = Minutes (0-59)
    //       Data[3] = Always '4' - Unknown meaning.
    case 0x256:
      curDayOfWeek = Message.data[0];
      curHours = Message.data[1];
      curMinutes = Message.data[2];
      break;

    //[DHW] - [RC] - Setpoint water temperature (Continuous-Flow Mode)
    //Data Type: INT
    //Value: Data / 2.0
    case 0x255:
      temp = Message.data[0] / 2.0;
      break;

    //[MC] - [Controller] - Mixed-Circuit Pump Operation
    //Data Type: Bit
    //Value: 1 = On | 0 = Off
    case 0x404:
      mcPump = Message.data[0];
      break;

    //[MC] - [RC] - Setpoint Mixed-Circuit Feed Temperature
    //Data Type: INT
    //Value: Data / 2.0
    case 0x405:
      temp = Message.data[0] / 2.0;
      break;

    //[MC] - [RC] - Mixed-Circuit Economy Setting
    //Data Type: Bit
    //Value: 1 = On | 0 = Off
    case 0x407:
      mcEconomy = Message.data[0];
      break;

    //[MC] - [Controller] - Mixed-Circuit Current Feed Temperature
    //Data Type: INT
    //Value: Data / 2.0
    case 0x440:
      temp = Message.data[0] / 2.0;
      break;
    }
  }
}
