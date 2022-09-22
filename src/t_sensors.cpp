#include <Arduino.h>
#include <t_sensors.h>
#include <mqtt.h>
#include <main.h>
#include <telnet.h>
#include <heating.h>
#include <configuration.h>

//-- Temperature Sensor Setup
// OneWire Pin
const int ONE_WIRE_BUS = 15;
// Precision of readings
const int TEMPERATURE_PRECISION = 12;

// Setup a OneWire instance
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

void initSensors()
{
    //Initialize the Dallas library
    sensors.begin();
    //Set Resolution for all attached sensors
    for (size_t i = 0; i < configuration.TemperatureSensors.SensorCount; i++)
    {
        sensors.setResolution(configuration.TemperatureSensors.Sensors[i].Address, TEMPERATURE_PRECISION);
    }

}

//Reads the attached temperature sensors and puts them into ceraValues.Auxilary
void ReadTemperatures()
{
    char printBuf[255];
    for (size_t i = 0; i < configuration.TemperatureSensors.SensorCount; i++)
    {
        sensors.requestTemperatures();
        float value = sensors.getTempC(configuration.TemperatureSensors.Sensors[i].Address);
        if (value != DEVICE_DISCONNECTED_C)
        {
            ceraValues.Auxilary.Temperatures[i] = value;
            //Set Return Feed Reference
            if (configuration.TemperatureSensors.Sensors[i].UseAsReturnValueReference)
            {
                ceraValues.Auxilary.FeedReturnTemperatureReference = value;
            }
            
            if (Debug)
            {
                Log.printf("DEBUG TEMP READING: %s Sensor: %.2f °C\r\n", configuration.TemperatureSensors.Sensors[i].Label, value);
            }
        }
        else
        {
            if (Debug)
            {
                Log.printf("DEBUG TEMP READING: %s Sensor is not reachable!\r\n", configuration.TemperatureSensors.Sensors[i].Label);
            }
        }
    }
}