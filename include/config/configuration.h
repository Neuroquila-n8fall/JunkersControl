#ifndef CONFIGURATION_H
#define CONFIGURATION_H

//——————————————————————————————————————————————————————————————————————————————
//  MQTT
//——————————————————————————————————————————————————————————————————————————————

// Topic where Parameters (settings) are received
extern char ParametersTopic[];

// Topic where heating temperatures are published at
extern char HeatingTemperaturesTopic[];
// Topic where water temperatures are published at
extern char WaterTemperaturesTopic[];
// Topic where temperatures of external temperature sensors are published at
extern char AuxilaryTemperaturesTopic[];
// Topic where the status should be published at
extern char StatusTopic[];

//——————————————————————————————————————————————————————————————————————————————
//  Feature Configuration
//——————————————————————————————————————————————————————————————————————————————

// Whether external temperature sensors are enabled
extern bool AuxSensorsEnabled;
// Whether heating temperatures should be published
extern bool HeatingTemperaturesEnabled;
// Whether water temperatures should be published
extern bool WaterTemperaturesEnabled;



#endif // CONFIGURATION_H
