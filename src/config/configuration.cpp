//——————————————————————————————————————————————————————————————————————————————
//  MQTT
//——————————————————————————————————————————————————————————————————————————————

char ParametersTopic[] = "cerasmarter/parameters";

char HeatingTemperaturesTopic[] = "cerasmarter/heating/parameters";

char WaterTemperaturesTopic[] = "cerasmarter/water/parameters";

char AuxilaryTemperaturesTopic[] = "cerasmarter/auxilary/parameters";

char StatusTopic[] = "cerasmarter/status";

//——————————————————————————————————————————————————————————————————————————————
//  Feature Configuration
//——————————————————————————————————————————————————————————————————————————————

bool AuxSensorsEnabled;

bool HeatingTemperaturesEnabled;

bool WaterTemperaturesEnabled;