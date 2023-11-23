#include "FakePWM.h"

FakePWM::FakePWM(pin_size_t pin)
{
    m_pin = pin;
}

void FakePWM::begin()
{
    pinMode(m_pin, OUTPUT);
}

void FakePWM::write(int value)
{
    m_level = value;
    update();
}

void FakePWM::update()
{
    uint64_t c = micros() % m_cycleus;
    int v = c * AHIGH / m_cycleus;
    digitalWrite(m_pin, v < m_level ? LOW : HIGH);
}
