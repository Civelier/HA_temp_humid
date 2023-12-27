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
#include <FlashStorage_SAMD.h>

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

    bool isValid()
    {
        return id == MEMORY_ID;
    }
};

struct MainConfig
{
    char wifi_ssid[WL_SSID_MAX_LENGTH+1];
    char wifi_password[WL_WPA_KEY_MAX_LENGTH+1];
    char ha_username[32];
    char ha_password[32];
    char ha_broker[64];
    char name[MAX_NAME_LENGTH];
    char temp_name[32];
    char humid_name[32];
    float temp_offset;
    float humid_offset;

    void printTo(Print& p)
    {
        p.print("SSID: "); p.println(wifi_ssid);
        p.print("Password: "); p.println(wifi_password);
        p.print("Username: "); p.println(ha_username);
        p.print("Password: "); p.println(ha_password);
        p.print("Broker: "); p.println(ha_broker);
        p.print("Name: "); p.println(name);
        p.print("Temp name: "); p.println(temp_name);
        p.print("Humid name: "); p.println(humid_name);
        p.print("Temp offset: "); p.println(temp_offset);
        p.print("Humid offset: "); p.println(humid_offset);
    }
};

MainConfig mainConfig;

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
        auto v = event.temperature + mainConfig.temp_offset;
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
        humidv = event.relative_humidity + mainConfig.humid_offset;
        DEBUG_STREAM.print(humidv);
        DEBUG_STREAM.println(F("%"));
        humid.setValue(event.relative_humidity);
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
    static bool first = true;
    if (WiFi.status() == wl_status_t::WL_CONNECTED && WiFi.localIP() != IPAddress(0,0,0,0))
    {
        lastWifiConnection = millis();
        if (next_rssiUpdate < millis())
        {
            int lvl = RSSI_LEVEL(WiFi.RSSI());
            ui.SetWifiState(lvl);
            next_rssiUpdate = millis() + RSSI_REFRESH;
            DEBUG_STREAM.print("RSSI: "); DEBUG_STREAM.println(WiFi.RSSI());
        }
        return true;
    }
    if (millis() - lastWifiConnection > (WIFI_TIMEOUT * 1000))
    {
        ui.DrawError(ERR_WIFI_TIMED_OUT);
        safeDelay(3000);
        exit(1);
    }
    
    if (!first) WiFi.end();
    else first = false;

    ui.SetWifiState(0);

    // connect to wifi
    WiFi.begin(mainConfig.wifi_ssid, mainConfig.wifi_password);
    
    DEBUG_STREAM.print(".");
    safeDelay(2000); // waiting for the connection
    return false;
}

void updateProgress()
{
    sodaq_wdt_reset();
    ui.DrawProgress(bootSteps[bootStepNumber], bootStepNumber + 1, 4);
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
        update();
        if (millis() - lastHaConnection > (HA_TIMEOUT * 1000))
        {
            ui.DrawError(ERR_HA_TIMED_OUT);
            safeDelay(3000);
            exit(1);
        }
    }
}

void loadConfig()
{
    mainConfig = MainConfig{
        WIFI_SSID,
        WIFI_PASSWORD,
        HA_USERNAME,
        HA_PASSWORD,
        BROKER_ADDRESS,
        NAME,
        TEMP_NAME,
        HUMID_NAME,
        TEMP_OFFSET,
        HUMID_OFFSET
    };
}

void setup()
{
    sodaq_wdt_enable(WDT_PERIOD_2X);

    // Setup Wire
    Wire.begin();
    sodaq_wdt_reset();

    // Setup LCD
    LCD.init();
    LCD.clear();
    LCD.backlight();
    ui.Setup();
    updateProgress();

    sodaq_wdt_reset();

    // Setup Serial
    DEBUG_STREAM.begin(9600);
    for (int i = 0; !DEBUG_STREAM && i < 50; i++) sodaq_wdt_safe_delay(100);
    DEBUG_STREAM.println("Starting...");

    statusLED.begin();
    
    loadConfig();
    mainConfig.printTo(DEBUG_STREAM);

    // Unique ID must be set!
    byte mac[WL_MAC_ADDR_LENGTH];
    WiFi.macAddress(mac);
    device.setUniqueId(mac, sizeof(mac));

    
    updateProgress();
    dht.begin();
    dht.temperature().printSensorDetails();
    dht.humidity().printSensorDetails();
    
    updateProgress();

    WiFi.setFeedWatchdogFunc(&update);
    WiFi.feedWatchdog();
    lastWifiConnection = millis();
    while (!ensureConnected()) safeDelay(100);

    // Set device's details (optional)
    device.setName(mainConfig.name);
    device.setSoftwareVersion(VERSION);

    updateProgress();
    mqtt.begin(mainConfig.ha_broker, mainConfig.ha_username, mainConfig.ha_password);

    // Set temp details
    // temp.setDeviceClass("temperature");
    temp.setName(mainConfig.temp_name);
    temp.setIcon("mdi:thermometer");
    temp.setUnitOfMeasurement("°C");
    

    // Set humid details
    // humid.setDeviceClass("humidity");
    humid.setName(mainConfig.humid_name);
    humid.setIcon("mdi:water-percent");
    humid.setUnitOfMeasurement("%");

    lastHaConnection = millis();
    while (!mqtt.isConnected()) 
    {
        if (ensureConnected()) mqttUpdate();
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
