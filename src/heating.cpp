#include <Arduino.h>
#include <heating.h>
#include <mqtt.h>
#include <templates.h>
#include <telnet.h>
#include <t_sensors.h>

//——————————————————————————————————————————————————————————————————————————————
//  Constants for heating control
//——————————————————————————————————————————————————————————————————————————————

//Basepoint for linear temperature calculation
const int calcHeatingBasepoint = -15;

//Endpoint for linear temperature calculation
const int calcHeatingEndpoint = 21;

//Minimum Feed Temperature. This is the base value for calculations. Setting this as the setpoint will trigger the economy mode.
const int calcTriggerAntiFreeze = 10;

//-- Heating Scheduler. Fallback values for when the MQTT broker isn't available
HeatingScheduleEntry fallbackStartEntry = {5, 30, 0, true};
HeatingScheduleEntry fallbackEndEntry = {23, 30, 0, false};

//-- Temperature
double temp = 0.00F;

//-- Heating Circuit: Max. Feed Temperature (From Controller)
double hcMaxFeed = 75.00F;

//-- Heating Circuit: Current Feed Temperature (From Controller)
double hcCurrentFeed = 0.00F;

//-- Heating Circuit: Feed Temperature Setpoint (Control Value)
double hcFeedSetpoint = 40.00F;

//-- Heating Controller: Current Temperature on the Outside (From Controller)
double OutsideTemperatureSensor = 0.00F;

//-- Heating Controller: Flame lit (From Controller)
bool flame = false;

//-- Heating Controller: Status. 0x0 = Operational. Error Flags vary between models!
uint8_t status = 0x00;

//-- Heating Circuit: Circulation Pump on|off
bool hcPump = false;

//-- Hot Water Battery Mode Status
bool hwBatteryMode = false;

//-- Heating Seasonal Mode. True = Summer | False = Winter
bool hcSeason = false;

//-- Mixed-Circuit Pump Status
bool mcPump = false;

//-- Mixed-Circuit Economy Setting
bool mcEconomy = false;

//-- Current Day of the week as configured by the CC
int curDayOfWeek = 0;

//-- Current Hour component
int curHours = 0;

//-- Current Minutes component
int curMinutes = 0;

//-- Heating active / operational
bool hcActive = true;

//-- Analog value of heating power
int hcHeatingPower = 0;

//-- Hot Water "now" setting.
bool hwNow = false;

//-- Max. possible water temperature -or- target temperature when running in heating battery mode
double hcMaxWaterTemperature = 0.0F;

//-- Current Water Temperature
double hcCurrentWaterTemperature = 0.0F;

//-- Fallback Values

//Basepoint Temperature
double fallbackBasepointTemperature = -10.0F;
//Endpoint Temperature
double fallbackEndpointTemperature = 31.0F;
//Ambient Temperature
double fallbackAmbientTemperature = 17.0F;
//Minimum ("Anti Freeze") Temperature.
double fallbackMinimumFeedTemperature = 10.0F;
//Enforces the fallback values when set to TRUE
bool isOnFallback = false;

//-- Takes the parameters and calculates the desired feed temperature.
//   This calculation takes in the desired base temperature at which the heating
//   should perform 100% of the reported maximum feed temperature and when it should perform at the minimum temperature.
//   The calculation maps endpoint and basepoint to minimum temperature and maximum feed temperature
//   The original controller takes a different approach: It lets you decide which temperature should be set when the outside temperature is -15°C or -20°C for some models
//   The calculation for the original controller is: map 25° and -15° to Off-Temperature and maximum temperature.
double CalculateFeedTemperature()
{
    char printbuf[255];
    //Map the current ambient temperature to the desired feed temperature:
    //        Ambient Temperature input, Endpoint i.e. 25°, Base Point i.e. -15°, Minimum Temperature at 25° i.e. 10°, Maximum Temperature at -15° i.e. maximum feed temperature the heating is capable of.
    if (!hcActive)
    {
        if (isOnFallback)
        {
            return fallbackMinimumFeedTemperature;
        }
        else
        {
            return mqttMinimumFeedTemperature;
        }
    }

    //Force setpoint set via MQTT
    if (mqttOverrideSetpoint)
    {
        return mqttFeedTemperatureSetpoint;
    }

    //The map() function will freeze the ESP if the values are the same.
    //  We will return the base value instead of calculating.
    if (mqttBasepointTemperature == mqttEndpointTemperature)
    {
        return mqttBasepointTemperature;
    }

    //Return max feed temperature when boost is requested.
    if (mqttBoost)
    {
        return hcMaxFeed;
    }

    //Scale Feed Temperature with current "demand" based upon valve opening
    //Fast Heatup will be ignored and disabled, if active, in this case because the most open valve is dominant.
    if (mqttValveScaling)
    {
        //Disable Fast Heatup when this option is enabled.
        if (mqttFastHeatup)
        {
            mqttFastHeatup = false;
        }

        double scaledTemp = 0.0;
        //Calculate dynamic adaption if requested.
        if (mqttDynamicAdaption)
        {
            //Map outside temperature beginning at basepoint to endpoint, to 0 and the adaption value
            //This means that at mqttBasepointTemperature the adaption will equal mqttFeedAdaption and 0 when OutsideTemperature equals mqttEndpointTemperature
            double dynamicAdaption = map_Generic(OutsideTemperatureSensor, mqttBasepointTemperature, mqttEndpointTemperature, 0, mqttFeedAdaption);
            //This will map the current opening from the possible range (0 to mqttMaxValveOpening) to the defined temperature range, starting from the "anti freeze" temp added with the adaption value.
            scaledTemp = map_Generic(mqttValveOpening, 0, mqttMaxValveOpening, mqttMinimumFeedTemperature + dynamicAdaption, hcMaxFeed);
            if (Debug)
            {
                sprintf(printbuf, "DEBUG SET TEMP: Valve Scaled + Dyn. Adapt.: %.2f (Including %.2f Adaption)  \r\n", scaledTemp, dynamicAdaption);
                String message(printbuf);
                WriteToConsoles(message);
            }
        }
        //Otherwise calculate the temperature based upon the fixed adaption value
        else
        {
            //This will map the current opening from the possible range (0 to mqttMaxValveOpening) to the defined temperature range, starting from the "anti freeze" temp added with the adaption value.
            scaledTemp = map_Generic(mqttValveOpening, 0, mqttMaxValveOpening, mqttMinimumFeedTemperature + mqttFeedAdaption, hcMaxFeed);
            if (Debug)
            {
                sprintf(printbuf, "DEBUG SET TEMP: Valve Scaled + Static Adapt.: %.2f (Including %.2f Adaption)  \r\n", scaledTemp, mqttFeedAdaption);
                String message(printbuf);
                WriteToConsoles(message);
            }
        }

        //Round value to half steps
        double halfRoundedVScale = llround(scaledTemp * 2) / 2.0;
        //...return value.
        return halfRoundedVScale;
    }

    //Map Value
    double linearTemp = map_Generic(OutsideTemperatureSensor, mqttEndpointTemperature, mqttBasepointTemperature, mqttMinimumFeedTemperature, hcMaxFeed);

    //Dynamic Adaption Feature. Only active when fast heatup is inactive!
    if (mqttDynamicAdaption && !mqttFastHeatup)
    {
        double dynamicAdaption = mqttTargetAmbientTemperature - aux_returnTemperature;
        //In this case we add the manual adaption.
        dynamicAdaption += mqttFeedAdaption;
        // ... then we add everything up
        linearTemp += dynamicAdaption;
    }
    else
    {
        //Add Manual Adaption
        linearTemp += mqttFeedAdaption;
    }

    //Round value to half steps
    double halfRounded = llround(linearTemp * 2) / 2.0;

    //Return max feed temperature if fast heatup is on and the ambient temperature hasn't been reached yet.
    if (mqttFastHeatup)
    {
        //Target Temperature hasn't been reached
        if (mqttAmbientTemperature < mqttTargetAmbientTemperature)
        {

            //Let's tune this so the heating hasn't to work that hard all the way
            //  We can take the temperature difference as reference and map the max feed temperature accordingly
            //First we calculate the difference between desired temperature (target) and actual value
            double tempDiff = mqttTargetAmbientTemperature - mqttAmbientTemperature;

            //Sanity check: max feed equals calculated
            if (hcMaxFeed == linearTemp)
            {
                //Bail out.
                return hcMaxFeed;
            }

            //This is the initial temperature difference when heatup started. referenceAmbientTemperature is set as soon as mqttFastHeatup is activated
            double initialReferenceDifference = mqttTargetAmbientTemperature - mqttReferenceAmbientTemperature;

            //Note: We don't have to check the target and current ambient temperature as this case is already handled by the initial comparison

            //Now we map the difference, which is decreasing over time, to the fixed range between reference (=starting point) and target ambient
            //  We map it according to the maximum available feed temperature and the currently calculated temperature.
            //Previous checks will prevent any divisions by zero that would otherwise stop the MC from operating
            double fhTemp = map_Generic(tempDiff, 0, initialReferenceDifference, linearTemp, hcMaxFeed);
            //Expected Values:
            //Room Temperature is 17°C
            //Target is 21°C
            //Difference is 4°C
            //Calculated Feed is 50°
            //Max is 75°C
            //Result: ( ( 4   -   0 )   ×   ( 75   -   50 ) )   ÷   ( 4   -   0 )   +   50 = 75

            //Room Temperature is 21°C
            //Target is 21°C
            //Difference is 0°C
            //Calculated Feed is 50°
            //Max is 75°C
            //Result: ( ( 0   -   0 )   ×   ( 75   -   50 ) )   ÷   ( 0   -   0 )   +   50 = 50
            //Half-Step-Round
            double hrFhTemp = llround(fhTemp * 2) / 2.0;

            if (Debug)
            {
                sprintf(printbuf, "DEBUG SET TEMP: Fast Heatup is active. Current: %.2f Target: %.2f Setpoint is %.2f \r\n", mqttAmbientTemperature, mqttTargetAmbientTemperature, fhTemp);
                String message(printbuf);
                WriteToConsoles(message);
            }

            //Check if the calculated temperature is higher than the reported maximum feed temperature.
            if (hrFhTemp > hcMaxFeed)
            {
                if (Debug)
                {
                    WriteToConsoles("DEBUG SET TEMP: Fast Heatup is active. Calculated Temperature is higher than the maximum possible! \r\n");
                }
                //Something went wrong. We should play it safe and return the default value instead.
                return linearTemp;
            }

            //Return Result.
            return hrFhTemp;
        }
        //If we reached the goal, set it to false again so it won't trigger again when the temperature drops
        else
        {
            mqttFastHeatup = false;
        }
    }

    if (Debug)
    {

        sprintf(printbuf, "DEBUG MAP VALUE: %.2f >> from %.2f to %.2f to %.2f and %.2f >> %.2f >> Half-Step Round: %.2f", OutsideTemperatureSensor, mqttEndpointTemperature, mqttBasepointTemperature, mqttMinimumFeedTemperature, hcMaxFeed, linearTemp, halfRounded);
        String message(printbuf);
        WriteToConsoles(message + "\r\n");
    }
    return halfRounded;
}

//Converts the value to its half-step representation (= value times two)
int ConvertFeedTemperature(double temperature)
{
    return temperature * 2;
}