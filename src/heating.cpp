#include <Arduino.h>
#include <heating.h>
#include <mqtt.h>
#include <templates.h>
#include <telnet.h>
#include <t_sensors.h>

CeraValues ceraValues;

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
    if (!ceraValues.Heating.Active)
    {
        if (ceraValues.Fallback.isOnFallback)
        {
            return ceraValues.Fallback.MinimumFeedTemperature;
        }
        else
        {
            return ceraValues.Heating.FeedMinimum;
        }
    }

    //Force setpoint set via MQTT
    if (commandedValues.Heating.OverrideSetpoint)
    {
        return commandedValues.Heating.FeedSetpoint;
    }

    //The map() function will freeze the ESP if the values are the same.
    //  We will return the base value instead of calculating.
    if (commandedValues.Heating.BasepointTemperature == commandedValues.Heating.EndpointTemperature)
    {
        return commandedValues.Heating.BasepointTemperature;
    }

    //Return max feed temperature when boost is requested.
    if (commandedValues.Heating.Boost)
    {
        
        return ceraValues.Heating.FeedMaximum;
    }

    //Scale Feed Temperature with current "demand" based upon valve opening
    //Fast Heatup will be ignored and disabled, if active, in this case because the most open valve is dominant.
    if (commandedValues.Heating.ValveScaling)
    {
        //Disable Fast Heatup when this option is enabled.
        if (commandedValues.Heating.FastHeatup)
        {
            commandedValues.Heating.FastHeatup = false;
        }

        double scaledTemp = 0.0;
        //Calculate dynamic adaption if requested.
        if (commandedValues.Heating.DynamicAdaption)
        {
            
            //Map outside temperature beginning at basepoint to endpoint, to 0 and the adaption value
            //This means that at commandedValues.Heating.BasepointTemperature the adaption will equal commandedValues.Heating.FeedAdaption and 0 when OutsideTemperature equals commandedValues.Heating.EndpointTemperature
            double dynamicAdaption = map_Generic(ceraValues.General.OutsideTemperature, commandedValues.Heating.BasepointTemperature, commandedValues.Heating.EndpointTemperature, 0, commandedValues.Heating.FeedAdaption);
            //This will map the current opening from the possible range (0 to commandedValues.Heating.MaxValveOpening) to the defined temperature range, starting from the "anti freeze" temp added with the adaption value.
            scaledTemp = map_Generic(commandedValues.Heating.ValveOpening, 0, commandedValues.Heating.MaxValveOpening, commandedValues.Heating.MinimumFeedTemperature + dynamicAdaption, ceraValues.Heating.FeedMaximum);
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
            //This will map the current opening from the possible range (0 to commandedValues.Heating.MaxValveOpening) to the defined temperature range, starting from the "anti freeze" temp added with the adaption value.
            scaledTemp = map_Generic(commandedValues.Heating.ValveOpening, 0, commandedValues.Heating.MaxValveOpening, commandedValues.Heating.MinimumFeedTemperature + commandedValues.Heating.FeedAdaption, ceraValues.Heating.FeedMaximum);
            if (Debug)
            {
                sprintf(printbuf, "DEBUG SET TEMP: Valve Scaled + Static Adapt.: %.2f (Including %.2f Adaption)  \r\n", scaledTemp, commandedValues.Heating.FeedAdaption);
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
    double linearTemp = map_Generic(ceraValues.General.OutsideTemperature, commandedValues.Heating.EndpointTemperature, commandedValues.Heating.BasepointTemperature, commandedValues.Heating.MinimumFeedTemperature, ceraValues.Heating.FeedMaximum);

    //Dynamic Adaption Feature. Only active when fast heatup is inactive!
    if (commandedValues.Heating.DynamicAdaption && !commandedValues.Heating.FastHeatup)
    {
        double dynamicAdaption = commandedValues.Heating.TargetAmbientTemperature - ceraValues.Auxilary.ReturnTemperature;
        //In this case we add the manual adaption.
        dynamicAdaption += commandedValues.Heating.FeedAdaption;
        // ... then we add everything up
        linearTemp += dynamicAdaption;
    }
    else
    {
        //Add Manual Adaption
        linearTemp += commandedValues.Heating.FeedAdaption;
    }

    //Round value to half steps
    double halfRounded = llround(linearTemp * 2) / 2.0;

    //Return max feed temperature if fast heatup is on and the ambient temperature hasn't been reached yet.
    if (commandedValues.Heating.FastHeatup)
    {
        //Target Temperature hasn't been reached
        if (commandedValues.Heating.AmbientTemperature < commandedValues.Heating.TargetAmbientTemperature)
        {

            //Let's tune this so the heating hasn't to work that hard all the way
            //  We can take the temperature difference as reference and map the max feed temperature accordingly
            //First we calculate the difference between desired temperature (target) and actual value
            double tempDiff = commandedValues.Heating.TargetAmbientTemperature - commandedValues.Heating.AmbientTemperature;

            //Sanity check: max feed equals calculated
            if (ceraValues.Heating.FeedMaximum == linearTemp)
            {
                //Bail out.
                return ceraValues.Heating.FeedMaximum;
            }

            //This is the initial temperature difference when heatup started. referenceAmbientTemperature is set as soon as commandedValues.Heating.FastHeatup is activated
            double initialReferenceDifference = commandedValues.Heating.TargetAmbientTemperature - commandedValues.Heating.ReferenceAmbientTemperature;

            //Note: We don't have to check the target and current ambient temperature as this case is already handled by the initial comparison

            //Now we map the difference, which is decreasing over time, to the fixed range between reference (=starting point) and target ambient
            //  We map it according to the maximum available feed temperature and the currently calculated temperature.
            //Previous checks will prevent any divisions by zero that would otherwise stop the MC from operating
            double fhTemp = map_Generic(tempDiff, 0, initialReferenceDifference, linearTemp, ceraValues.Heating.FeedMaximum);
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
                sprintf(printbuf, "DEBUG SET TEMP: Fast Heatup is active. Current: %.2f Target: %.2f Setpoint is %.2f \r\n", commandedValues.Heating.AmbientTemperature, commandedValues.Heating.TargetAmbientTemperature, fhTemp);
                String message(printbuf);
                WriteToConsoles(message);
            }

            //Check if the calculated temperature is higher than the reported maximum feed temperature.
            if (hrFhTemp > ceraValues.Heating.FeedMaximum)
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
            commandedValues.Heating.FastHeatup = false;
        }
    }

    if (Debug)
    {

        sprintf(printbuf, "DEBUG MAP VALUE: %.2f >> from %.2f to %.2f to %.2f and %.2f >> %.2f >> Half-Step Round: %.2f", ceraValues.General.OutsideTemperature, commandedValues.Heating.EndpointTemperature, commandedValues.Heating.BasepointTemperature, commandedValues.Heating.MinimumFeedTemperature, ceraValues.Heating.FeedMaximum, linearTemp, halfRounded);
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