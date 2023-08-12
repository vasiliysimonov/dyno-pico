#include "EMA.h"

EMA::EMA(uint32_t period) : m_period(period) {
    reset(float());
}

void EMA::update(float measurement) {
    if (m_count < m_period) ++m_count;
    m_value += (measurement - m_value) / m_count;
}

void EMA::reset(float v) {
    m_value = v;
    m_count = 0;
}

float EMA::get() {
    return m_value;
}
