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
void ReadTemperatures(void *pvParameters)
{
    while (true)
    {
        sensors.requestTemperatures();
        for (size_t i = 0; i < configuration.TemperatureSensors.SensorCount; i++)
        {
            
            float value = sensors.getTempC(configuration.TemperatureSensors.Sensors[i].Address);
            if (value != DEVICE_DISCONNECTED_C)
            {
                configuration.TemperatureSensors.Sensors[i].reachable = true;
                // Set Return Feed Reference
                if (configuration.TemperatureSensors.Sensors[i].UseAsReturnValueReference)
                {
                    ceraValues.Auxilary.FeedReturnTemperatureReference = value;
                }

                if (DebugMode)
                {
                    Log.printf("DEBUG TEMP READING: %s Sensor: %.2f °C\r\n", configuration.TemperatureSensors.Sensors[i].Label, value);
                }
            }
            else
            {
                ceraValues.Auxilary.Temperatures[i] = 0.0F;
                configuration.TemperatureSensors.Sensors[i].reachable = false;
                if (DebugMode)
                {
                    Log.printf("DEBUG TEMP READING: %s Sensor is not reachable!\r\n", configuration.TemperatureSensors.Sensors[i].Label);
                }
            }
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}