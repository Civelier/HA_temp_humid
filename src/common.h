#pragma once
#include <Arduino.h>
#include "secrets.h"

#define DHT_PIN 2
#define CFG_E_PIN 11
#define CFG_R_PIN 12
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
#define RSSI_LEVEL(_rssi) min(1, max(3, map(_rssi, -50,-85, 3, 1)))
#define RSSI_REFRESH 5000
#define MAX_NAME_LENGTH 14

// Error messages
#define ERR_INVALID_FLASH "R FLASH"
#define ERR_WIFI_TIMED_OUT "WIFI TIMED OUT"
#define ERR_HA_TIMED_OUT "HA TIMED OUT"

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

#define DEBUG_STREAM emptyStream