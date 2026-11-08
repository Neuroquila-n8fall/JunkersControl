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


//-- Auxiliary Sensor Values (OneWire Sensors)
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
        for (int i = 0; i < sensorDeviceCount; i++)
        {
          sensors.getAddress(foundDevice, i);
          sensors.setResolution(foundDevice, TEMPERATURE_PRECISION);
          value = sensors.getTempC(foundDevice);
          LogInfo("Sensor %d: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X = %.2f °C", i + 1,
                  foundDevice[0], foundDevice[1], foundDevice[2], foundDevice[3],
                  foundDevice[4], foundDevice[5], foundDevice[6], foundDevice[7], value);
        }
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
extern void ReadTemperatures(void *pvParameters);

//Initialize Sensors
extern void initSensors();

#endif
