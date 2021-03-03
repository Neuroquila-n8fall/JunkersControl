#ifndef _T_SENSORS_H
#define _T_SENSORS_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//-- Temperature Sensor Setup
// OneWire Pin
const int ONE_WIRE_BUS = 15;
// Precision of readings
const int TEMPERATURE_PRECISION = 12;

// Setup a OneWire instance
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Sensor Addresses
// HINT: Plug them in sperately and write down the addresses

//Use the following functions to print out a list of sensors
/*
    void QuerySensorsAndPrint()
    {
        int sensorDeviceCount = 1 (number of sensors on the bus)
        sensors.requestTemperatures();
        Serial.println("Printing addresses...");
        for (int i = 0; i < sensorDeviceCount; i++)
        {
          Serial.print("Sensor ");
          Serial.print(i + 1);
          Serial.print(" : ");
          sensors.getAddress(foundDevice, i);
          sensors.setResolution(foundDevice, TEMPERATURE_PRECISION);
          printAddress(foundDevice);
          value = sensors.getTempC(foundDevice);
          Serial.println(value);
        }
    }

    void printAddress(DeviceAddress deviceAddress)
    {
     for (uint8_t i = 0; i < 8; i++)
     {
       Serial.print("0x");
       if (deviceAddress[i] < 0x10)
         Serial.print("0");
       Serial.print(deviceAddress[i], HEX);
       if (i < 7)
         Serial.print(", ");
     }
     Serial.println("");
    }



*/

// arrays to hold device addresses. Adapt to your design!
DeviceAddress feed_sens = {0x28, 0x76, 0x51, 0x91, 0x42, 0x20, 0x01, 0xE3},
              return_sens = {0x28, 0x6D, 0x98, 0xF5, 0x42, 0x20, 0x01, 0x0F},
              exhaust_sens = {0x28, 0x6F, 0x9C, 0xF6, 0x42, 0x20, 0x01, 0xF6},
              ambient_sens = {0x28, 0xBF, 0x39, 0x10, 0x42, 0x20, 0x01, 0x93};

#endif