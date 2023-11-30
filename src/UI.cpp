#include "UI.h"


#if HAS_SCREEN

static_assert(sizeof(NAME) <= 14, "NAME has to have less than 14 chars.");

#define NO_WIFI_CHAR 0
byte noWifi_char[] = {
    0x11,
    0x0A,
    0x04,
    0x0A,
    0x11,
    0x04,
    0x0A,
    0x04
};

#define WIFI_CHAR1 1
byte wifi_char1[] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x04,
    0x00
};

#define WIFI_CHAR2 2
byte wifi_char2[] = {
    0x00,
    0x00,
    0x00,
    0x04,
    0x0A,
    0x00,
    0x04,
    0x00
};

#define WIFI_CHAR3 3
byte wifi_char3[] = {
    0x00,
    0x0E,
    0x11,
    0x04,
    0x0A,
    0x00,
    0x04,
    0x00
};

#define DISCONNECTED_CHAR 4
byte disconnected_char[] = {
    0x08,
    0x08,
    0x14,
    0x10,
    0x01,
    0x05,
    0x02,
    0x02
};

#define CONNECTED_CHAR 5
byte connected_char[] = {
    0x08,
    0x08,
    0x14,
    0x16,
    0x0D,
    0x05,
    0x02,
    0x02
};

#define DEGREE_CHAR 6
byte degree_char[] = {
    0x00,
    0x02,
    0x05,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00
};


UI::UI(LiquidCrystal_I2C &lcd)
{
    m_lcd = &lcd;
}

void UI::Setup()
{
    m_lcd->createChar(NO_WIFI_CHAR, noWifi_char);
    m_lcd->createChar(WIFI_CHAR1, wifi_char1);
    m_lcd->createChar(WIFI_CHAR2, wifi_char2);
    m_lcd->createChar(WIFI_CHAR3, wifi_char3);
    m_lcd->createChar(DISCONNECTED_CHAR, disconnected_char);
    m_lcd->createChar(CONNECTED_CHAR, connected_char);
    m_lcd->createChar(DEGREE_CHAR, degree_char);
    // m_lcd->noDisplay();
    m_lcd->clear();
    m_lcd->home();
    m_lcd->print(NAME);
    // m_lcd->display();
}

#else
UI::UI()
{
}
void UI::Setup()
{
}
#endif


void UI::DrawProgress(const char *stepName, int step, int total)
{
    #if HAS_SCREEN
    // m_lcd->noDisplay();
    ClearLine(1);
    m_lcd->setCursor(0, 1);
    m_lcd->print(stepName);
    m_lcd->print(' ');
    m_lcd->print(step);
    m_lcd->print('/');
    m_lcd->print(total);
    // m_lcd->display();
    #endif
    DEBUG_STREAM.print(stepName);
    DEBUG_STREAM.print(' ');
    DEBUG_STREAM.print(step);
    DEBUG_STREAM.print('/');
    DEBUG_STREAM.println(total);
}

void UI::DrawWidgets()
{
    #if HAS_SCREEN
    // m_lcd->noDisplay();
    m_lcd->setCursor(14, 0);
    m_lcd->print("  ");
    m_lcd->setCursor(14, 0);
    switch (m_wifiLevel)
    {
    case 0:
        m_lcd->write(NO_WIFI_CHAR);
        break;
    case 1:
        m_lcd->write(WIFI_CHAR1);
        break;
    case 2:
        m_lcd->write(WIFI_CHAR2);
        break;
    case 3:
        m_lcd->write(WIFI_CHAR3);
        break;
    default:
        m_lcd->write(NO_WIFI_CHAR);
        break;
    }
    if (m_haConnected) m_lcd->write(CONNECTED_CHAR);
    else m_lcd->write(DISCONNECTED_CHAR);
    // m_lcd->display();
    #endif
}

void UI::DrawValues(float temp, float humid)
{
    #if HAS_SCREEN
    // m_lcd->noDisplay();
    ClearLine(1);
    // 0    5   9    15
    // XX.X °C  XX.X %
    m_lcd->setCursor(0, 1);
    m_lcd->print(temp, 1);
    m_lcd->setCursor(5, 1);
    m_lcd->write(DEGREE_CHAR);
    m_lcd->print('C');
    m_lcd->setCursor(9, 1);
    m_lcd->print(humid, 1);
    m_lcd->setCursor(15, 1);
    m_lcd->print("%");
    // m_lcd->display();
    #else
    DEBUG_STREAM.print("Temp: ");
    DEBUG_STREAM.print(temp, 1);
    DEBUG_STREAM.print(" °C\tHumid: ");
    DEBUG_STREAM.print(humid, 1);
    DEBUG_STREAM.println(" %");
    #endif
}

void UI::DrawError(const char *msg)
{
    #if HAS_SCREEN
    // m_lcd->noDisplay();
    ClearLine(1);
    m_lcd->setCursor(0,1);
    m_lcd->print(msg);
    m_lcd->flush();
    // m_lcd->display();
    #endif
    DEBUG_STREAM.print("[ERROR] ");
    DEBUG_STREAM.println(msg);
}

void UI::DrawInfo(const char *msg)
{
    #if HAS_SCREEN
    // m_lcd->noDisplay();
    ClearLine(1);
    m_lcd->setCursor(0,1);
    m_lcd->print(msg);
    m_lcd->flush();
    // m_lcd->display();
    #endif
    DEBUG_STREAM.print("[INFO] ");
    DEBUG_STREAM.println(msg);
}

void UI::SetWifiState(int level)
{
    if (level == m_wifiLevel) return;
    m_wifiLevel = level;
    #if HAS_SCREEN
    DrawWidgets();
    #endif
    if (level) DEBUG_STREAM.println("WIFI connected");
    else DEBUG_STREAM.println("WIFI disconnected");
}

void UI::SetHaState(bool connected)
{
    if (connected == m_haConnected) return;
    m_haConnected = connected;
    #if HAS_SCREEN
    DrawWidgets();
    #endif
    if (connected) DEBUG_STREAM.println("HA connected");
    else DEBUG_STREAM.println("HA disconnected");
}

#if HAS_SCREEN
void UI::ClearLine(uint8_t row)
{
    m_lcd->setCursor(0,row);
    m_lcd->print("                ");
}
#endif