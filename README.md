#JunkersControl
##Table of contents
- [Purpose and Aim](#purpose-and-aim)
  - [Cerasmart-er](#cerasmart-er)
- [Contribution](#contribution)
- [Intended Audience](#intended-audience)
- [A word of warning](#a-word-of-warning)
  - [But why? We are talking about a data line!](#but-why-we-are-talking-about-a-data-line)
- [Prerequisites](#prerequisites)
- [Features](#features)
- [Hints](#hints)
- [Todo](#todo)
- [Special Thanks](#special-thanks)

## Purpose and Aim
This project is designed around the idea of having a SCADA-like setup where your command & control server (MQTT-Broker) sends commands and receives the status of the heating.
Since the rise of modern and affordable "Smart Radiator Thermostats" we are able to precisely control the room temperature whereas the usual central heating system is only able to react on outside temperatures and doesn't know what the actual demand is like. The heating is only capable of reacting to certain drops in feed and return temperatures and heat up according to setpoints defined by the consumer. This principle is proven and hasn't changed until today.

### Cerasmart-er
The Junkers/Bosch "Heatronic" controller of the "Cerasmart" type of boilers (yes they used that term back in pre-2000!) is by itself smart enough to keep your home warm without wasting too much energy, if properly equipped and configured, of course. You won't receive any benefits from this project if you are messing around with the parameters without knowing the concepts of a central heating system.
**This also means you can't tell the boiler to perform unreasonable actions because the controller inside the boiler unit is in charge of controlling the actual temperatures within a safe, predefined range that has been set either by the manufacturer or the technician that maintains your heating.**
We are, hoewever, able to suggest certain temperatures or switch the boiler on and off

With this project we can at least account for seasonal changes in temperature, humidity and the temperature as we feel it so we can adjust the power demand to what we actually need.


## Contribution
There are a lot of different possible setups around and I am happy to accept PRs no matter what they are about. 

Just a few examples:


- You have found an Id and its meaning
- Something is explained wrong
- Bugs, of course

## Intended Audience
Since every setup is different you have to customize the data processing for yourself. I have sourced the message ids from https://www.mikrocontroller.net/topic/81265 but only process those that are relevant to me.
This means you should bring a little bit of coding experience with you.


## A word of warning
You should never, ever manipulate a device that's running on highly volatile substances if you don't know what you are doing. Nobody, in fact, is qualified to work on such a gas heating system but the trained technician that has the right tools and knowledge to modify your heating in a safe manner (read: without blowing up you, himself and the surrounding home). 
If your are missing a cable to plug in your self-made controller, don't install it yourself. Ask your qualified, local plumber to do it.
### But why? We are talking about a data line!
 Simply because installing the bus module or attaching the data cable requires you to open the boiler, remove the cover and also the bezel that covers up the mains voltage supply. In some countries this will also void any insurance coverage if you do it yourself.

## Prerequisites
Now that we have sorted out the serious bits, lets check if we got everything together to pull this off...

1) A compatible Bosch-Junkers central gas heating system with Bosch Heatronic controller and BM1 or BM2 Bus Module equipped.
2) Access to the data line that exits the bus module. Most of the time you will find a "room thermostat" like TA250 or TA270 which in fact is the control unit for you, the consumer. **It won't work with 1-2-4 style room thermostats like the TR200**
Again, when in doubt, ask a technician.
3) Awareness to short circuits and bus failures due to wrong wiring
4) Direct access to the heating itself in case of problems.
5) No, really, you shouldn't mess with things that aren't **yours**
6) Ideally an ESP32 Kit but if you are just interested in the CAN-Stuff you can of course throw away all the MQTT and WiFi and just use a bog standard arduino.
7) A MCP2515 + TJA1050 Can-Bus module (i.e. branded "NiRen")
8) A MQTT broker (i.e. Mosquitto)

## Features
- Control the parameters like base and endpoint values which are responsible for selecting the right feed temperature. This will calculate the required feed temperature like the original.
- Because we are in full control over the feed temperature we can also account for weather, humidity and actual room temperatures, if we like.
- Control over the circulation pump of either hot water and heating
- Report of parameters the heating is working with like current feed temperature, maximum feed temperature, outside temperature, hot water temperature
- OTA Updates and "console" over Telnet so you can see what messages are sent on the bus to further improve your setup
- Fallback/Failsafe mode which will return to hardcoded values in case the connection to the "mother ship" (MQTT) has been lost for whatever reason.

## Hints
- If you just wanna read then you have to set the variable `Override` to `false`. This way nothing will be sent on the bus but you can read everything.
- For debug purposes the `Debug` variable controls wether you want to see verbose output of the underlying routines like feed temperature calculation and step chain progress.
- Keep in mind that if you are intending to migrate this to an arduino you have to watch out for the OTA feature and `float` (`%f`) format parameters within `sprintf` calls.
- When OTA is triggered, all connections will be terminated except the one used for OTA because otherwise the update will fail. The MC will keep working.
- The OTA feature is confirmed working with Arduino IDE and Platform.io but for the latter you have to adapt the settings inside `platformio.ini` to your preference.

## Todo
- [x] Find a suitable CAN module and library that is able to handle 10kbit/s using the ESP32
- [x] Debug output over Telnet
- [x] OTA Update Capability
- [x] Try not to get mad while searching for the reason why the OTA update is aborting at around 2-8%
- [x] Collecting IDs and their meaning
- [x] Getting the calculations right for the feed setpoint
- [x] Reading and writing MQTT topics
- [x] Fallback Mode
- [ ] Taking Weather conditions into account when calculating the feed temperature
- [ ] Also taking indoor temperatures into account
- [x] Getting the timings right so it doesn't throw off the controller
- [x] Testing as a standalone solution
- [ ] Example Configuration for Home Assistant

## Special Thanks
- The people at the mikrocontroller.net forums
- Pierre Molinaro and contributors of the ACAN2515 library: https://github.com/pierremolinaro/acan2515
- Nick O'Leary and contributors of the PubSubClient library: https://github.com/knolleary/pubsubclient
- Rop Gonggrijp and contributors of the ezTime library: https://github.com/ropg/ezTime
