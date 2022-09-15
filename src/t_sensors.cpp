#include <Arduino.h>
#include <t_sensors.h>
#include <mqtt.h>
#include <main.h>
#include <telnet.h>
#include <heating.h>

//-- Temperature Sensor Setup
// OneWire Pin
const int ONE_WIRE_BUS = 15;
// Precision of readings
const int TEMPERATURE_PRECISION = 12;

// Setup a OneWire instance
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses. Adapt to your design!
DeviceAddress feed_sens = {0x28, 0x76, 0x51, 0x91, 0x42, 0x20, 0x01, 0xE3};
DeviceAddress return_sens = {0x28, 0x6F, 0x9C, 0xF6, 0x42, 0x20, 0x01, 0xF6};
DeviceAddress exhaust_sens = {0x28, 0x6D, 0x98, 0xF5, 0x42, 0x20, 0x01, 0x0F};
DeviceAddress ambient_sens = {0x28, 0xBF, 0x39, 0x10, 0x42, 0x20, 0x01, 0x93};

void initSensors()
{
    //Initialize the Dallas library
    sensors.begin();
    //Set Resolution for all attached sensors
    sensors.setResolution(feed_sens, TEMPERATURE_PRECISION);
    sensors.setResolution(return_sens, TEMPERATURE_PRECISION);
    sensors.setResolution(exhaust_sens, TEMPERATURE_PRECISION);
    sensors.setResolution(ambient_sens, TEMPERATURE_PRECISION);
}

//Reads the attached temperature sensors and publishes the values to MQTT
void ReadTemperatures()
{
    sensors.requestTemperatures();
    char printBuf[255];
    float value = sensors.getTempC(feed_sens);
    if (value != DEVICE_DISCONNECTED_C)
    {
        ceraValues.Auxilary.FeedTemperature = value;
        if (Debug)
        {
            sprintf(printBuf, "DEBUG TEMP READING: Feed Sensor: %.2f °C\r\n", value);
            String message(printBuf);
            WriteToConsoles(message);
        }
    }
    else
    {
        if (Debug)
        {
            WriteToConsoles("DEBUG TEMP READING: Feed Sensor is not reachable!\r\n");
        }
    }
    sensors.requestTemperatures();
    value = sensors.getTempC(return_sens);
    if (value != DEVICE_DISCONNECTED_C)
    {
        ceraValues.Auxilary.ReturnTemperature = value;
        if (Debug)
        {
            sprintf(printBuf, "DEBUG TEMP READING: Return Sensor: %.2f °C\r\n", value);
            String message(printBuf);
            WriteToConsoles(message);
        }
    }
    else
    {
        if (Debug)
        {
            WriteToConsoles("DEBUG TEMP READING: Return Sensor is not reachable!\r\n");
        }
    }
    sensors.requestTemperatures();
    value = sensors.getTempC(exhaust_sens);
    if (value != DEVICE_DISCONNECTED_C)
    {
        ceraValues.Auxilary.ExhaustTemperature = value;
        if (Debug)
        {
            sprintf(printBuf, "DEBUG TEMP READING: Exhaust Sensor: %.2f °C\r\n", value);
            String message(printBuf);
            WriteToConsoles(message);
        }
    }
    else
    {
        if (Debug)
        {
            WriteToConsoles("DEBUG TEMP READING: Exhaust Sensor is not reachable!\r\n");
        }
    }
    sensors.requestTemperatures();
    value = sensors.getTempC(ambient_sens);
    if (value != DEVICE_DISCONNECTED_C)
    {
        ceraValues.Auxilary.AmbientTemperature = value;
        if (Debug)
        {
            sprintf(printBuf, "DEBUG TEMP READING: Ambient Sensor: %.2f °C\r\n", value);
            String message(printBuf);
            WriteToConsoles(message);
        }
    }
    else
    {
        if (Debug)
        {
            WriteToConsoles("DEBUG TEMP READING: Ambient Sensor is not reachable!\r\n");
        }
    }
}