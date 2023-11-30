#pragma once
#include <Arduino.h>
#include "secrets.h"

#define DHT_PIN 2
#define CFG_PIN 3
#define LCD_ADDRESS 0x27

#if SENSOR_TYPE == 0
    #define REFRESH_MS 1000
#endif

#if SENSOR_TYPE == 1
    #define REFRESH_MS 2000
#endif

#define AHIGH 255
#define ALOW 0
#define BREATHE_TIME 5000
#define WIFI_TIMEOUT 60
#define HA_TIMEOUT 120
#define RSSI_LEVEL(_rssi) map(_rssi, -50,-85, 3, 1);
#define RSSI_REFRESH 5000

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

static EmptyStream emptyStream = EmptyStream();

#define DEBUG_STREAM Serial