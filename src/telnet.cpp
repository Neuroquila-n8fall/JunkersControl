#include <Arduino.h>
#include <telnet.h>
#include <WiFi.h>
#include <wifi_config.h>
#include <configuration.h>

const uint TelnetServerPort = 23;
WiFiServer TelnetServer(TelnetServerPort);
WiFiClient TelnetRemoteClient;
ConsoleWriter Log;

//Checks for new Telnet connections
void CheckForConnections()
{
  //Check if we're having a new client connection
  if (TelnetServer.hasClient())
  {
    //Check if we already have a client
    if (TelnetRemoteClient.connected())
    {
      Serial.println("Telnet Connection rejected");
      //Reject connection.
      TelnetServer.available().stop();
    }
    else
    {
      Serial.println("Telnet Connection accepted");
      //Accept
      TelnetRemoteClient = TelnetServer.available();
      TelnetRemoteClient.println("——————————————————————————");
      TelnetRemoteClient.printf("You are connected to: %s (%s)\r\n", configuration.Wifi.Hostname, WiFi.localIP().toString().c_str());
      TelnetRemoteClient.println("——————————————————————————");
    }
  }
}

//Read commands from Telnet clients.
void ReadFromTelnet()
{
  //Very basic implementation. Does the job but isn't perfect...
  if (TelnetRemoteClient.connected() && TelnetRemoteClient.available())
  {
    String command = TelnetRemoteClient.readStringUntil('\r');
    command.trim();
    command.toLowerCase();

    //Do nothing on empty command.
    if (command.length() == 0)
    {
      return;
    }

    if (strcmp(command.c_str(), "reboot") == 0)
    {
      TelnetRemoteClient.println("Reboot in 5 seconds...");
      delay(5000);
      ESP.restart();
      return;
    }
  }
}

