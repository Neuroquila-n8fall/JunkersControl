/**
 * Non Volatile Memory - Permanent Storage
 */

#include <Preferences.h>
#include <nvm.h>
#include <heating.h>

void initSettings() {
    preferences.begin("Cerasmarter", false);

    loadSettings();
}

void loadSettings() {
    temp = preferences.getDouble("Temperature", 10.0F);

    hcMaxFeed = preferences.getDouble("hcMaxFeed", 75.0F);

    HkSollVorlauf = preferences.getDouble("HkSollVorlauf", 10.0F);

    fallbackBasepointTemperature = preferences.getDouble("fallbackBasepointTemperature", -10.0F);

    fallbackEndpointTemperature = preferences.getDouble("fallbackEndpointTemperature", 31.0F);

    fallbackAmbientTemperature = preferences.getDouble("fallbackAmbientTemperature", 17.0F);

    fallbackMinimumFeedTemperature = preferences.getDouble("fallbackMinimumFeedTemperature", 10.0F);
}
