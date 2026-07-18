#ifndef HA_AUTODISCOVERY_H
#define HA_AUTODISCOVERY_H

#include <Arduino.h>

void SetupHomeAssistantDiscovery();
void PublishHomeAssistantAvailability(bool online);
bool HandleHomeAssistantMessage(const char *topic, const String &payload);

#endif // HA_AUTODISCOVERY_H
