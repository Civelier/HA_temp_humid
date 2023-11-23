#pragma once

#include "common.h"

#if HAS_SCREEN
    #include <Wire.h>
    #include <LiquidCrystal_I2C.h>
#endif


class UI
{
private:
    #if HAS_SCREEN
    LiquidCrystal_I2C* m_lcd;
    #endif
    bool m_wifiConnected;
    bool m_haConnected;

public:
    #if HAS_SCREEN
    UI(LiquidCrystal_I2C& lcd);
    #else
    UI();
    #endif

    void Setup();
    void DrawProgress(const char* stepName, int step, int total);
    void DrawWidgets();
    void DrawValues(float temp, float humid);
    void DrawError(const char* msg);
    void SetWifiState(bool connected);
    void SetHaState(bool connected);
private:
    #if HAS_SCREEN
    void ClearLine(uint8_t row);
    #endif
};