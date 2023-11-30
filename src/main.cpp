// Arduino imports
#include <Arduino.h>
#include <Wire.h>
#include <WiFiNINA.h>

// Third party imports
#include "Sodaq_wdt.h"
#include <ArduinoHA.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <DHT_U.h>

// Local imports
#include "secrets.h"
#include "common.h"
#include "FakePWM.h"
#include "UI.h"

// Devices
FakePWM statusLED = FakePWM(LED_BUILTIN);
WiFiClient client = WiFiClient();
HADevice device = HADevice();
HAMqtt mqtt = HAMqtt(client, device);
HASensorNumber temp = HASensorNumber(TEMP_NAME, HASensorNumber::PrecisionP1);
HASensorNumber humid = HASensorNumber(HUMID_NAME, HASensorNumber::PrecisionP1);

typedef const char* c_str_t;

uint32_t next_update = 0;
uint32_t lastWDT = 0;
uint32_t next_rssiUpdate = 0;
uint32_t lastWifiConnection = 0;
uint32_t lastHaConnection = 0;
int bootStepNumber = 0;

c_str_t bootSteps[] = {
    "Init",
    "DHT",
    "WiFi",
    "MQTT",
    "Add temp",
    "Add humid",
};

// A random number used to validate the integrity of the values in InitialConfig
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

/**
 * @brief Pointer allocated with malloc to prevent overwriting the values stored in the SRAM. This value stays untouched when restarted.
 * */ 
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

/// @brief Low cost update for idle moments
void update()
{
    if (millis() - lastWDT > 1000)
    {
        sodaq_wdt_reset();
        lastWDT = millis();
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        statusLED.write((millis() % 2000) > 1000 ? ALOW : AHIGH);
        // int v = (int)(abs((float)(millis() % BREATHE_TIME) / ((float)BREATHE_TIME/2.0) - 1.0) * AHIGH);
        // statusLED.write(v);
    }
    else
    {
        statusLED.write((millis() % 200) > 100 ? ALOW : AHIGH);
    }

    statusLED.update();
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

void safeDelay(uint32_t ms)
{
    uint32_t end = millis() + ms;
    while (millis() < end) update();
}

bool ensureConnected()
{
    if (WiFi.status() == wl_status_t::WL_CONNECTED) 
    {
        lastWifiConnection = millis();
        if (next_rssiUpdate < millis())
        {
            int lvl = RSSI_LEVEL(WiFi.RSSI());
            ui.SetWifiState(lvl);
            next_rssiUpdate = millis() + RSSI_REFRESH;
        }
        return true;
    }
    if (millis() - lastWifiConnection > WIFI_TIMEOUT) 
    {
        ui.DrawError("WIFI TIMED OUT");
        safeDelay(3000);
        exit(1);
    }
    ui.SetWifiState(0);

    // connect to wifi
    const char* wifi_ssid = WIFI_SSID;
    const char* wifi_password = WIFI_PASSWORD;
    WiFi.begin(wifi_ssid, wifi_password);
    DEBUG_STREAM.print(".");
    safeDelay(500); // waiting for the connection
    return false;
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
    }
    else
    {
        ui.SetHaState(false);
        
        // DEBUG_STREAM.print('.');
        // DEBUG_STREAM.flush();
        if (millis() - lastHaConnection > HA_TIMEOUT) 
        {
            ui.DrawError("HA TIMED OUT");
            safeDelay(3000);
            exit(1);
        }
    }
}

void setup()
{
    config = (InitialConfig*)malloc(sizeof(InitialConfig));
    sodaq_wdt_enable(WDT_PERIOD_2X);

    Wire.begin();
    sodaq_wdt_reset();
    // Setup LCD
    LCD.init();
    LCD.clear();
    LCD.backlight();
    ui.Setup();
    updateProgress();

    sodaq_wdt_reset();
    DEBUG_STREAM.begin(9600);
    for (int i = 0; !DEBUG_STREAM && i < 50; i++) sodaq_wdt_safe_delay(100);
    DEBUG_STREAM.println("Starting...");

    
    

    // Unique ID must be set!
    byte mac[WL_MAC_ADDR_LENGTH];
    WiFi.macAddress(mac);
    device.setUniqueId(mac, sizeof(mac));

    statusLED.begin();
    
    updateProgress();
    dht.begin();
    dht.temperature().printSensorDetails();
    dht.humidity().printSensorDetails();
    
    updateProgress();

    lastWifiConnection = millis();
    while (!ensureConnected());

    // Set device's details (optional)
    device.setName(NAME);
    device.setSoftwareVersion(VERSION);

    


    updateProgress();
    mqtt.begin(BROKER_ADDRESS, HA_USERNAME, HA_PASSWORD);

    if (!config->tempInitialized())
    {
        // Set temp details
        // temp.setDeviceClass("temperature");
        temp.setName(TEMP_NAME);
        temp.setIcon("mdi:thermometer");
        temp.setUnitOfMeasurement("°C");
        

        // Set humid details
        // humid.setDeviceClass("humidity");
        humid.setName(HUMID_NAME);
        humid.setIcon("mdi:water-percent");
        humid.setUnitOfMeasurement("%");
    }
    else
    {
        // Set humid details
        // humid.setDeviceClass("humidity");
        humid.setName(HUMID_NAME);
        humid.setIcon("mdi:water-percent");
        humid.setUnitOfMeasurement("%");
        

        // Set temp details
        // temp.setDeviceClass("temperature");
        temp.setName(TEMP_NAME);
        temp.setIcon("mdi:thermometer");
        temp.setUnitOfMeasurement("°C");
    }

    lastHaConnection = millis();
    while (!mqtt.isConnected()) mqttUpdate();

    updateProgress();
    if (!config->tempInitialized())
    {
        config->id = MEMORY_ID;
        mqtt.addDeviceType(&temp);
        temp.setAvailability(true);

        next_update = millis() + 5000;
        while (millis() < next_update)
        {
            temp.setValue(0.0f, true);
            mqttUpdate();
            delay(5);
        }
        config->tempInit = true;
        config->humidInit = false;
        ui.DrawInfo("Restarting...");
        delay(1000);
        for(;;);
    }

    updateProgress();
    if (!config->humidInitialized())
    {
        config->id = MEMORY_ID;
        mqtt.addDeviceType(&humid);
        humid.setAvailability(true);

        next_update = millis() + 5000;
        while (millis() < next_update)
        {
            humid.setValue(0.0f, true);
            mqttUpdate();
            delay(5);
        }
        config->humidInit = true;
        ui.DrawInfo("Restarting...");
        delay(1000);
        for(;;);
    }

    mqtt.addDeviceType(&humid);
    mqttUpdate();
    humid.setAvailability(true);
    mqttUpdate();
    mqtt.addDeviceType(&temp);
    mqttUpdate();
    temp.setAvailability(true);
    mqttUpdate();
    

    device.publishAvailability();
    next_update = millis();
}

void loop() 
{
    ensureConnected();
    mqttUpdate();
    update();

    
}
