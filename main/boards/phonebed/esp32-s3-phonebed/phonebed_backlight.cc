#include "phonebed_backlight.h"
#include <driver/ledc.h>
#include <esp_log.h>

#define TAG "PhonebedBacklight"

// Use TIMER_1 + CH_5 to avoid conflict with motor channels (TIMER_2 CH_0-3) and fan (TIMER_3 CH_4)
#define BL_TIMER  LEDC_TIMER_1
#define BL_CHANNEL LEDC_CHANNEL_5

PhonebedBacklight::PhonebedBacklight(gpio_num_t pin, bool output_invert, uint32_t freq_hz) : Backlight() {
    const ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = BL_TIMER,
        .freq_hz = freq_hz,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    const ledc_channel_config_t ch_cfg = {
        .gpio_num = pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BL_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BL_TIMER,
        .duty = 0,
        .hpoint = 0,
        .flags = {
            .output_invert = output_invert,
        }
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
}

PhonebedBacklight::~PhonebedBacklight() {
    ledc_stop(LEDC_LOW_SPEED_MODE, BL_CHANNEL, 0);
}

void PhonebedBacklight::SetBrightnessImpl(uint8_t brightness) {
    uint32_t duty_cycle = (1023 * brightness) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_CHANNEL, duty_cycle);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_CHANNEL);
}
