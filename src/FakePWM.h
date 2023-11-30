#pragma once

#include "common.h"

class FakePWM
{
    private:
    const uint64_t m_cycleus = 1000;
    pin_size_t m_pin;
    int m_level = 0;
    public:
    FakePWM(pin_size_t pin);
    

    void begin();

    void write(int value);

    void update();
};