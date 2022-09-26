#ifndef _T_SENSORS_H
#define _T_SENSORS_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//——————————————————————————————————————————————————————————————————————————————
//  Variables
//——————————————————————————————————————————————————————————————————————————————

//-- Temperature Sensor Setup
// OneWire Pin
extern const int ONE_WIRE_BUS;
// Precision of readings
extern const int TEMPERATURE_PRECISION;

// Setup a OneWire instance
extern OneWire oneWire;

// Pass our oneWire reference to Dallas Temperature.
extern DallasTemperature sensors;


//-- Auxilary Sensor Values (OneWire Sensors)
extern double aux_feedTemperature;
extern double aux_returnTemperature;
extern double aux_ambientTemperature;
extern double aux_exhaustEmperature;

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
extern DeviceAddress feed_sens;
extern DeviceAddress return_sens;
extern DeviceAddress exhaust_sens;
extern DeviceAddress ambient_sens;
//###

//——————————————————————————————————————————————————————————————————————————————
//  Functions
//——————————————————————————————————————————————————————————————————————————————

//Read Temperatures and send the results using MQTT
extern void ReadTemperatures(void *parameter);

//Initialize Sensors
extern void initSensors();

#endif
