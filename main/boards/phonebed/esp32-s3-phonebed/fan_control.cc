#include "fan_control.h"
#include "config.h"
#include "mcp_server.h"

#include <driver/ledc.h>
#include <esp_log.h>

#define TAG "FanControl"

// LEDC: TIMER_3, CH_4, 10kHz, 10-bit
// Fan PWM is inverted: duty 0 = full speed, duty 1023 = off
#define FAN_TIMER   LEDC_TIMER_3
#define FAN_CHANNEL LEDC_CHANNEL_4

static const int kSpeedTable[] = {
    1023,  // Level 0: OFF
    768,   // Level 1: LOW
    512,   // Level 2: MED-LOW
    256,   // Level 3: MED-HIGH
    0      // Level 4: HIGH (full speed)
};

FanControl::FanControl() {
    InitLedc();

    auto& mcp = McpServer::GetInstance();
    mcp.AddTool("self.fan.set_speed",
        "设置风扇速度 (Set fan speed: 0=off, 1=low, 2=medium-low, 3=medium-high, 4=high)",
        PropertyList({
            Property("level", kPropertyTypeInteger, 0, 4)
        }),
        [this](const PropertyList& props) -> ReturnValue {
            int level = props["level"].value<int>();
            SetSpeed(level);
            return std::string("风扇速度已设置为 " + std::to_string(level));
        });

    ESP_LOGI(TAG, "Fan control initialized");
}

void FanControl::InitLedc() {
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = FAN_TIMER,
        .freq_hz = 10000,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t ch_cfg = {
        .gpio_num = FAN_PWM_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = FAN_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = FAN_TIMER,
        .duty = 1023, // Start OFF
        .hpoint = 0,
        .flags = { .output_invert = 0 }
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
}

void FanControl::SetSpeed(int level) {
    if (level < 0) level = 0;
    if (level > 4) level = 4;
    speed_level_ = level;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, FAN_CHANNEL, kSpeedTable[level]);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, FAN_CHANNEL);
    ESP_LOGI(TAG, "Fan speed set to level %d (duty=%d)", level, kSpeedTable[level]);
}
