#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <IRremote.h>

#define RELAY_NO true
#define NUM_RELAYS 4
#define DEBOUNCE_DELAY 300
#define IR_RECEIVE_PIN 4

int relayGPIOs[NUM_RELAYS] = {26, 27, 25, 33};
int relayStates[NUM_RELAYS] = {0, 0, 0, 0};
int buttonGPIOs[NUM_RELAYS] = {16, 17, 5, 18};
unsigned long lastButtonPress[NUM_RELAYS] = {0};

int gpio2State = 0;

const char *ssid = "Switchify";
const char *password = "mahfujar";
const char *hostname = "switchify";

AsyncWebServer server(80);
Preferences preferences;

IRrecv irrecv(IR_RECEIVE_PIN);
decode_results results;

#define IR_CODE_1 0xFFA25D
#define IR_CODE_2 0xFF629D
#define IR_CODE_3 0xFFE21D
#define IR_CODE_4 0xFF22DD

void toggleRelay(int index)
{
    relayStates[index] = !relayStates[index];
    digitalWrite(relayGPIOs[index], RELAY_NO ? !relayStates[index] : relayStates[index]);

    preferences.begin("relayStates", false);
    preferences.putInt(String(index).c_str(), relayStates[index]);
    preferences.end();

    Serial.printf("Relay %d toggled\n", index + 1);
}

void setup()
{
    Serial.begin(115200);

    if (!SPIFFS.begin(true))
    {
        Serial.println("SPIFFS mount failed");
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

    for (int i = 0; i < NUM_RELAYS; i++)
    {
        pinMode(buttonGPIOs[i], INPUT_PULLUP);
    }

    WiFi.softAP(ssid, password);
    Serial.println("Access Point created");
    Serial.println(WiFi.softAPIP());

    if (MDNS.begin(hostname))
    {
        Serial.printf("mDNS started: http://%s.local\n", hostname);
    }
    else
    {
        Serial.println("mDNS failed");
    }

    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("relay") && request->hasParam("state")) {
            String relayParam = request->getParam("relay")->value();
            String stateParam = request->getParam("state")->value();
            int state = stateParam.toInt();

            if (relayParam == "gpio2") {
                if (state == 0 || state == 1) {
                    digitalWrite(2, state);
                    gpio2State = state;
                    Serial.printf("GPIO2 set to %d\n", state);
                }
            } else {
                int relayIndex = relayParam.toInt() - 1;
                if (relayIndex >= 0 && relayIndex < NUM_RELAYS && (state == 0 || state == 1)) {
                    relayStates[relayIndex] = state;
                    digitalWrite(relayGPIOs[relayIndex], RELAY_NO ? !state : state);

                    preferences.begin("relayStates", false);
                    preferences.putInt(String(relayIndex).c_str(), state);
                    preferences.end();

                    Serial.printf("Relay %d set to %d\n", relayIndex + 1, state);
                }
            }
        }
        request->send(200, "text/plain", "OK"); });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        JsonDocument json;
        for (int i = 0; i < NUM_RELAYS; i++) {
            json[String(i + 1)] = relayStates[i];
        }
        json["gpio2"] = gpio2State;

        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response); });

    // Start IR receiver
    irrecv.enableIRIn();

    server.begin();
}

void loop()
{
    unsigned long now = millis();

    // Button input handling with debounce
    for (int i = 0; i < NUM_RELAYS; i++)
    {
        if (digitalRead(buttonGPIOs[i]) == LOW && now - lastButtonPress[i] > DEBOUNCE_DELAY)
        {
            toggleRelay(i);
            lastButtonPress[i] = now;
        }
    }

    // IR remote handling with debounce and repeat filter
    static unsigned long lastIR = 0;
    if (irrecv.decode(&results))
    {
        if (results.value != 0xFFFFFFFF && now - lastIR > 500)
        {
            Serial.printf("IR code: 0x%lX\n", results.value);

            switch (results.value)
            {
            case IR_CODE_1:
                toggleRelay(0);
                break;
            case IR_CODE_2:
                toggleRelay(1);
                break;
            case IR_CODE_3:
                toggleRelay(2);
                break;
            case IR_CODE_4:
                toggleRelay(3);
                break;
            }

            lastIR = now;
        }
        irrecv.resume();
    }
}
