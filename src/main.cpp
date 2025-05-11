#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ESPmDNS.h>

#define RELAY_NO true
#define NUM_RELAYS 4

int relayGPIOs[NUM_RELAYS] = {26, 27, 25, 33};
int relayStates[NUM_RELAYS] = {0, 0, 0, 0};
int gpio2State = 0;

const char *ssid = "espSwitch";
const char *password = "mahfujar";
const char *hostname = "switch";

AsyncWebServer server(80);
Preferences preferences;

void setup()
{
    Serial.begin(115200);

    if (!SPIFFS.begin(true))
    {
        Serial.println("An error occurred while mounting SPIFFS");
        return;
    }

    preferences.begin("relayStates", true);
    for (int i = 0; i < NUM_RELAYS; i++)
    {
        pinMode(relayGPIOs[i], OUTPUT);
        relayStates[i] = preferences.getInt(String(i).c_str(), 0);
        digitalWrite(relayGPIOs[i], RELAY_NO ? !relayStates[i] : relayStates[i]);
    }
    preferences.end();

    pinMode(2, OUTPUT);
    gpio2State = 0;
    digitalWrite(2, gpio2State);

    WiFi.softAP(ssid, password);
    Serial.println("Access Point created");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    if (!MDNS.begin(hostname))
    {
        Serial.println("Error starting mDNS");
        return;
    }
    Serial.printf("mDNS responder started: http://%s.local\n", hostname);

    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("relay") && request->hasParam("state"))
        {
            String inputMessage = request->getParam("relay")->value();
            String inputMessage2 = request->getParam("state")->value();

            if (inputMessage == "gpio2")
            {
                int state = inputMessage2.toInt();
                if (state == 0 || state == 1)
                {
                    digitalWrite(2, state);
                    gpio2State = state;
                    Serial.printf("GPIO2 set to %d\n", state);
                }
                else
                {
                    Serial.println("Invalid state for GPIO2");
                }
            }
            else
            {
                int relayIndex = inputMessage.toInt() - 1;
                int state = inputMessage2.toInt();

                if (relayIndex >= 0 && relayIndex < NUM_RELAYS && (state == 0 || state == 1))
                {
                    digitalWrite(relayGPIOs[relayIndex], RELAY_NO ? !state : state);
                    relayStates[relayIndex] = state;

                    preferences.begin("relayStates", false);
                    preferences.putInt(String(relayIndex).c_str(), state);
                    preferences.end();

                    Serial.printf("Relay %d set to %d\n", relayIndex + 1, state);
                }
                else
                {
                    Serial.println("Invalid relay index or state");
                }
            }
        }
        else
        {
            Serial.println("Missing parameters: relay or state");
        }

        request->send(200, "text/plain", "OK"); });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        JsonDocument json;
        for (int i = 0; i < NUM_RELAYS; i++)
        {
            json[String(i + 1)] = relayStates[i];
        }
        json["gpio2"] = gpio2State;

        String jsonString;
        serializeJson(json, jsonString);
        request->send(200, "application/json", jsonString); });

    server.begin();
}

void loop()
{
}
