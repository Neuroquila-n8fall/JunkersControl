#ifndef FORECAST_TEST_MQTT_H
#define FORECAST_TEST_MQTT_H
struct ForecastTestCommandedValues
{
    struct HeatingValues
    {
        double CalculatedFeedSetpoint = 10.0;
    } Heating;
};
extern ForecastTestCommandedValues commandedValues;
void PublishHeatingTemperaturesAndStatus();
#endif
