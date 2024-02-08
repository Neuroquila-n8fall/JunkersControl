#if !defined(HA_AUTODISCOVERY_H)
#define HA_AUTODISCOVERY_H

#include <Arduino.h>
#include <configuration.h>
#include <mqtt.h>

extern const char *HaSensorsFileName;
extern const char *HaBinarySensorsFileName;
extern const char *HaNumbersFileName;

extern void SetupAutodiscovery(const char* fileName);
extern void sendMQTTTemperatureDiscoveryMsg();
extern void CreateAndPublishAutoDiscoverySensorJson(
    String name,
    String value_template,
    String unit_of_measurement,
    String state_topic,
    char *device_class,
    bool force_update,
    char *sensorShortName
);

#endif // HA_AUTODISCOVERY_H
