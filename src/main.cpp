#include <Arduino.h>
#include <WiFiNINA.h>
#include <ArduinoHA.h>

#define BROKER_ADDR     IPAddress(192,168,0,17)
#define WIFI_SSID       "MyNetwork"
#define WIFI_PASSWORD   "MyPassword"


WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device);

void setup() 
{
    Serial.begin(9600);
    Serial.println("Starting...");

    // Unique ID must be set!
    byte mac[WL_MAC_ADDR_LENGTH];
    WiFi.macAddress(mac);
    device.setUniqueId(mac, sizeof(mac));

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // connect to wifi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500); // waiting for the connection
    }
    Serial.println();
    Serial.println("Connected to the network");
    digitalWrite(LED_BUILTIN, HIGH);


    // set device's details (optional)
    device.setName("Nano 33 IoT");
    device.setSoftwareVersion("1.0.0");
}

void loop() 
{

}
