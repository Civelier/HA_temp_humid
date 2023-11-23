#include <Arduino.h>
#include "secrets.h"
#include "common.h"
#include "FakePWM.h"
#include <WiFiNINA.h>
#include <ArduinoHA.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "UI.h"
#include <DHT.h>
#include <DHT_U.h>
#include "Sodaq_wdt.h"

FakePWM statusLED = FakePWM(4);
WiFiClient client = WiFiClient();
HADevice device = HADevice();
HAMqtt mqtt = HAMqtt(client, device);
HASensorNumber temp = HASensorNumber(TEMP_NAME, HASensorNumber::PrecisionP1);
HASensorNumber humid = HASensorNumber(HUMID_NAME, HASensorNumber::PrecisionP1);

#define MEMORY_ID 478295

struct InitialConfig
{
    bool tempInit;
    bool humidInit;
    int id;
    bool tempInitialized()
    {
        if (id == MEMORY_ID) return tempInit;
        return false;
    }

    bool humidInitialized()
    {
        if (id == MEMORY_ID) return humidInit;
        return false;
    }
};

InitialConfig* config = nullptr;

#if HAS_SCREEN
    LiquidCrystal_I2C LCD = LiquidCrystal_I2C(LCD_ADDRESS, 16, 2);
    UI ui = UI(LCD);
#else
    UI ui = UI();
#endif

#if SENSOR_TYPE == 0
    DHT_Unified dht = DHT_Unified(DHT_PIN, DHT11);
#endif

#if SENSOR_TYPE == 1
    DHT_Unified dht = DHT_Unified(DHT_PIN, DHT22);
#endif


typedef const char* c_str_t;

uint32_t next_update = 0;
uint32_t lastWifiConnection = 0;
uint32_t lastHaConnection = 0;
int bootStepNumber = 0;

c_str_t bootSteps[] = {
    "Init",
    "WiFi",
    "MQTT",
    "Add temp",
    "Add humid",
    "DHT",
};

void ensureConnected()
{
    if (WiFi.status() == wl_status_t::WL_CONNECTED) 
    {
        lastWifiConnection = millis();
        return;
    }
    if (millis() - lastWifiConnection > WIFI_TIMEOUT) 
    {
        ui.DrawError("WIFI TIMED OUT");
        sodaq_wdt_safe_delay(3000);
        exit(1);
    }
    ui.SetWifiState(false);
    statusLED.write(ALOW);

    // connect to wifi
    const char* wifi_ssid = WIFI_SSID;
    const char* wifi_password = WIFI_PASSWORD;
    WiFi.begin(wifi_ssid, wifi_password);
    while (WiFi.status() != WL_CONNECTED) {
        DEBUG_STREAM.print(".");
        sodaq_wdt_safe_delay(500); // waiting for the connection
    }
    // DEBUG_STREAM.println();
    // DEBUG_STREAM.println("Connected to the network");
    ui.SetWifiState(true);
    statusLED.write(AHIGH);
}

void updateProgress()
{
    sodaq_wdt_reset();
    ui.DrawProgress(bootSteps[bootStepNumber], bootStepNumber + 1, 6);
    ui.DrawWidgets();
    bootStepNumber++;
}

void mqttUpdate()
{
    mqtt.loop();
    sodaq_wdt_reset();
    if (mqtt.isConnected())
    {
        lastHaConnection = millis();
        ui.SetHaState(true);
        int v = (int)(abs((float)(millis() % BREATHE_TIME) / ((float)BREATHE_TIME/2.0) - 1.0) * AHIGH);
        statusLED.write(v);
    }
    else
    {
        ui.SetHaState(false);
        analogWrite(LED_BUILTIN, (millis() % 1000) > 500 ? ALOW : AHIGH);
        // DEBUG_STREAM.print('.');
        // DEBUG_STREAM.flush();
        if (millis() - lastHaConnection > HA_TIMEOUT) 
        {
            ui.DrawError("HA TIMED OUT");
            sodaq_wdt_safe_delay(3000);
            exit(1);
        }
    }
}

void setup()
{
    config = (InitialConfig*)malloc(sizeof(InitialConfig));
    DEBUG_STREAM.begin(9600);
    for (int i = 0; !DEBUG_STREAM && i < 50; i++) sodaq_wdt_safe_delay(100);
    sodaq_wdt_enable(WDT_PERIOD_2X);

    DEBUG_STREAM.println(config->id);
    DEBUG_STREAM.println(config->tempInit);
    DEBUG_STREAM.println(config->humidInit);

    Wire.begin();

    sodaq_wdt_reset();
    DEBUG_STREAM.println("Starting...");
    // Setup LCD
    LCD.init();
    DEBUG_STREAM.println("Lcd init...");
    DEBUG_STREAM.flush();
    LCD.clear();
    LCD.backlight();
    ui.Setup();
    sodaq_wdt_reset();


    updateProgress();

    // Unique ID must be set!
    byte mac[WL_MAC_ADDR_LENGTH];
    WiFi.macAddress(mac);
    device.setUniqueId(mac, sizeof(mac));

    statusLED.begin();
    
    updateProgress();

    lastWifiConnection = millis();
    ensureConnected();

    // Set device's details (optional)
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

    updateProgress();
    mqtt.begin(BROKER_ADDRESS, HA_USERNAME, HA_PASSWORD);
    lastHaConnection = millis();
    while (!mqtt.isConnected()) mqttUpdate();

    updateProgress();
    if (!config->tempInitialized())
    {
        config->id = MEMORY_ID;
        mqtt.addDeviceType(&temp);

        next_update = millis() + 2000;
        while (millis() < next_update)
        {
            temp.setValue(0.0f, true);
            mqttUpdate();
            delay(5);
        }
        config->tempInit = true;
        exit(0);
    }

    updateProgress();
    if (!config->humidInitialized())
    {
        config->id = MEMORY_ID;
        mqtt.addDeviceType(&humid);

        next_update = millis() + 2000;
        while (millis() < next_update)
        {
            humid.setValue(0.0f, true);
            mqttUpdate();
            delay(5);
        }
        config->humidInit = true;
        exit(0);
    }

    updateProgress();
    dht.begin();
    dht.temperature().printSensorDetails();
    dht.humidity().printSensorDetails();

    device.publishAvailability();
    next_update = millis();

    
}

void loop() 
{
    sodaq_wdt_reset();
    ensureConnected();
    statusLED.update();
    mqttUpdate();
    delay(5);

    if (millis() < next_update)
    {
        return;
    }
    next_update = millis() + REFRESH_MS;
    sensors_event_t event;
    // Get temperature event and print value.
    dht.temperature().getEvent(&event);
    float tempv = 0;
    float humidv = 0;
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
        tempv = v;
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
        humidv = event.relative_humidity;
    }
    ui.DrawValues(tempv, humidv);
}
