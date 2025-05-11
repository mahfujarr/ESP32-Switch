#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ESPmDNS.h>

#define RELAY_NO true
#define NUM_RELAYS 4

int relayGPIOs[NUM_RELAYS] = {26, 27, 25, 33};
int relayStates[NUM_RELAYS] = {0, 0, 0, 0}; // Initialize all relays to OFF
int gpio2State = 0;                         // State for GPIO2 (D2 LED)

const char *ssid = "espSwitch";    // SSID for the ESP32 AP
const char *password = "mahfujar"; // Password for the ESP32 AP
const char *hostname = "switch";   // mDNS hostname

AsyncWebServer server(80);
Preferences preferences; // Create Preferences object

void setup()
{
    Serial.begin(115200);

    // Initialize SPIFFS
    if (!SPIFFS.begin(true))
    {
        Serial.println("An error occurred while mounting SPIFFS");
        return;
    }

    // Initialize Preferences and load saved relay states
    preferences.begin("relayStates", true); // Open in read-only mode
    for (int i = 0; i < NUM_RELAYS; i++)
    {
        pinMode(relayGPIOs[i], OUTPUT);

        // Load the saved state from Preferences
        relayStates[i] = preferences.getInt(String(i).c_str(), 0); // Default to 0 (OFF) if not found
        digitalWrite(relayGPIOs[i], RELAY_NO ? !relayStates[i] : relayStates[i]);
    }

    // Initialize GPIO2 and load its saved state
    pinMode(2, OUTPUT);
    gpio2State = preferences.getInt("gpio2", 0); // Default to 0 (OFF) if not found
    digitalWrite(2, gpio2State);

    preferences.end();

    // Create Access Point
    WiFi.softAP(ssid, password);
    Serial.println("Access Point created");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    // Initialize mDNS
    if (!MDNS.begin(hostname))
    {
        Serial.println("Error starting mDNS");
        return;
    }
    Serial.printf("mDNS responder started: http://%s.local\n", hostname);

    // Serve static files from SPIFFS
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    // Handle relay updates
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String inputMessage;
        String inputMessage2;

        if (request->hasParam("relay") && request->hasParam("state"))
        {
            inputMessage = request->getParam("relay")->value();
            inputMessage2 = request->getParam("state")->value();

            if (inputMessage == "gpio2")
            {
                // Handle GPIO2 toggle
                int state = inputMessage2.toInt();
                if (state == 0 || state == 1)
                {
                    digitalWrite(2, state);
                    gpio2State = state;

                    // Save the state to Preferences
                    preferences.begin("relayStates", false); // Open in read-write mode
                    preferences.putInt("gpio2", state);
                    preferences.end();

                    Serial.printf("GPIO2 set to %d\n", state);
                }
                else
                {
                    Serial.println("Invalid state for GPIO2");
                }
            }
            else
            {
                // Handle relay toggle
                int relayIndex = inputMessage.toInt() - 1;
                int state = inputMessage2.toInt();

                if (relayIndex >= 0 && relayIndex < NUM_RELAYS && (state == 0 || state == 1))
                {
                    digitalWrite(relayGPIOs[relayIndex], RELAY_NO ? !state : state);
                    relayStates[relayIndex] = state; // Update the relay state

                    // Save the state to Preferences
                    preferences.begin("relayStates", false); // Open in read-write mode
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

    // Handle status requests
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