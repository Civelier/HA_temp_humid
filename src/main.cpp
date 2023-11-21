#include <Arduino.h>
#include <WiFiNINA.h>
#include <ArduinoHA.h>
#include "secrets.h"
#include <DHT.h>
#include <DHT_U.h>
#include <pins_arduino.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <string.h>

#define CONFIG_PATH "config.json"

#ifdef HAS_SD_CARD
    DynamicJsonDocument config(1024);
    #define BROKER_ADDRESS config["broker_address"]
    #undef WIFI_SSID
    #define WIFI_SSID config["wifi_ssid"]
    #undef WIFI_PASSWORD
    #define WIFI_PASSWORD config["wifi_password"]
    #undef HA_USERNAME
    #define HA_USERNAME config["ha_username"]
    #undef HA_PASSWORD
    #define HA_PASSWORD config["ha_password"]
    #define DHT_PIN config["dht_pin"]
    #define FILE_VERSION config["version"]

#else
    #define BROKER_ADDRESS "192.168.1.163"
    #define DHT_PIN 2
#endif

#if SENSOR_TYPE == 0
    #define REFRESH_MS 1000
#endif

#if SENSOR_TYPE == 1
    #define REFRESH_MS 2000
#endif

class EmptyStream : public Stream
{
public:
    EmptyStream() {}
    bool begin(int) { return true; }
    int available() override { return 0; }
    int read() override { return 0; }
    int peek() override { return 0; }
    size_t write(uint8_t) { return 0; }

    operator bool() { return false; }

};

EmptyStream emptyStream = EmptyStream();

#define DEBUG_STREAM Serial

WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device);

HASensorNumber temp(TEMP_NAME, HASensorNumber::PrecisionP1);
HASensorNumber humid(HUMID_NAME, HASensorNumber::PrecisionP1);

#if SENSOR_TYPE == 0
    DHT_Unified dht(DHT_PIN, DHT11);
#endif

#if SENSOR_TYPE == 1
    DHT_Unified dht(DHT_PIN, DHT22);
#endif

uint32_t next_update = 0;

#define AHIGH 255
#define ALOW 0
#define BREATHE_TIME 5000
#define TEMP_OFFSET (-3.0f)

class FakePWM
{
    private:
    const uint64_t m_cycleus = 10000;
    pin_size_t m_pin;
    int m_level = 0;
    public:
    FakePWM(pin_size_t pin)
    {
        m_pin = pin;
    }

    void begin()
    {
        pinMode(m_pin, OUTPUT);
    }

    void write(int value)
    {
        m_level = value;
        update();
    }

    void update()
    {
        uint64_t c = micros() % m_cycleus;
        int v = c * AHIGH / m_cycleus;
        digitalWrite(m_pin, v < m_level ? LOW : HIGH);
    }
};

FakePWM statusLED(LED_BUILTIN);

void ensureConnected()
{
    if (WiFi.status() == wl_status_t::WL_CONNECTED) return;
    statusLED.write(ALOW);

    // connect to wifi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        DEBUG_STREAM.print(".");
        delay(500); // waiting for the connection
    }
    DEBUG_STREAM.println();
    DEBUG_STREAM.println("Connected to the network");
    statusLED.write(AHIGH);
}

void saveConfig()
{
    auto file = SD.open(CONFIG_PATH, FILE_WRITE);
    serializeJsonPretty(config, file);
    file.close();
}

void defaultConfig()
{
    BROKER_ADDRESS = "";
    WIFI_SSID = "";
    WIFI_PASSWORD = "";
    HA_USERNAME = "";
    HA_PASSWORD = "";
    DHT_PIN = 2;
    saveConfig();
}

void readConfig()
{
    if (SD.exists(CONFIG_PATH))
    {
        auto file = SD.open(CONFIG_PATH, FILE_READ);
        deserializeJson(config, file);
        file.close();
        if (!strcmp(FILE_VERSION, VERSION))
        {
        }
    }
}

void setup() 
{
    #ifdef HAS_SD_CARD
    SD.begin()
    #endif
    DEBUG_STREAM.begin(9600);
    DEBUG_STREAM.println("Starting...");



    // Unique ID must be set!
    byte mac[WL_MAC_ADDR_LENGTH];
    WiFi.macAddress(mac);
    device.setUniqueId(mac, sizeof(mac));

    statusLED.begin();
    
    ensureConnected();

    // set device's details (optional)
    device.setName(NAME);
    device.setSoftwareVersion(VERSION);

    // Set temp details
    // temp.setDeviceClass("temperature");
    temp.setName("Temperature");
    temp.setIcon("mdi:thermometer");
    temp.setUnitOfMeasurement("°C");

    // Set humid details
    // humid.setDeviceClass("humidity");
    humid.setName("Humidity");
    humid.setIcon("mdi:water-percent");
    humid.setUnitOfMeasurement("%");


    mqtt.begin(BROKER_ADDRESS, HA_USERNAME, HA_PASSWORD);

    dht.begin();
    dht.temperature().printSensorDetails();
    dht.humidity().printSensorDetails();

    device.publishAvailability();
    next_update = millis();

    mqtt.addDeviceType(&temp);
    mqtt.addDeviceType(&humid);
}


void loop() 
{
    ensureConnected();
    statusLED.update();
    mqtt.loop();
    if (mqtt.isConnected())
    {
        int v = (int)(abs((float)(millis() % BREATHE_TIME) / ((float)BREATHE_TIME/2.0) - 1.0) * AHIGH);
        statusLED.write(v);
    }
    else
    {
        analogWrite(LED_BUILTIN, (millis() % 1000) > 500 ? ALOW : AHIGH);
        DEBUG_STREAM.print('.');
        DEBUG_STREAM.flush();
        delay(500);
        return;
    }

    if (millis() < next_update)
    {
        return;
    }
    next_update = millis() + REFRESH_MS;
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) 
    {
        DEBUG_STREAM.println(F("Error reading temperature!"));
    }
    else 
    {
        DEBUG_STREAM.print(F("Temperature: "));
        auto v = event.temperature + TEMP_OFFSET;
        DEBUG_STREAM.print(v);
        DEBUG_STREAM.println(F("°C"));
        temp.setValue(v);
    }
    // Get humidity event and print its value.
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) 
    {
        DEBUG_STREAM.println(F("Error reading humidity!"));
    }
    else 
    {
        DEBUG_STREAM.print(F("Humidity: "));
        DEBUG_STREAM.print(event.relative_humidity);
        DEBUG_STREAM.println(F("%"));
        humid.setValue(event.relative_humidity);
    }
}
