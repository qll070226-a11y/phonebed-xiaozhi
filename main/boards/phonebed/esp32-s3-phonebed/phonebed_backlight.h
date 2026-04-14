#pragma once

#include "backlight.h"
#include <driver/gpio.h>

// Custom backlight using LEDC TIMER_1 + CH_5 to avoid conflict with motors (TIMER_2 CH_0-3)
class PhonebedBacklight : public Backlight {
public:
    PhonebedBacklight(gpio_num_t pin, bool output_invert = false, uint32_t freq_hz = 25000);
    ~PhonebedBacklight();

    void SetBrightnessImpl(uint8_t brightness) override;
};
