#include "motor_control.h"
#include "config.h"
#include "mcp_server.h"

#include <driver/ledc.h>
#include <driver/gpio.h>
#include <esp_log.h>

#define TAG "MotorControl"

// LEDC: TIMER_2, CH_0-3, 1kHz, 10-bit
#define MOTOR_TIMER     LEDC_TIMER_2
#define MOTOR_FREQ_HZ   1000
#define MOTOR_RESOLUTION LEDC_TIMER_10_BIT

static const ledc_channel_t motor_channels[] = {
    LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3
};
static const gpio_num_t motor_pins[] = {
    MOTOR_PWM_PIN1, MOTOR_PWM_PIN2, MOTOR_PWM_PIN3, MOTOR_PWM_PIN4
};

MotorControl::MotorControl() {
    InitLedc();
    InitLimitSwitches();

    // Create limit monitor task (starts suspended)
    xTaskCreatePinnedToCore(LimitMonitorTask, "motor_limit", 2048, this, 10, &limit_task_, 1);
    vTaskSuspend(limit_task_);

    // Register MCP tools
    auto& mcp = McpServer::GetInstance();

    mcp.AddTool("self.quilt.open",
        "打开被子叫醒手机 (Open the quilt to wake up the phone)",
        PropertyList(),
        [this](const PropertyList&) -> ReturnValue {
            QuiltOpen();
            return std::string("被子正在打开");
        });

    mcp.AddTool("self.quilt.close",
        "盖好被子让手机睡觉 (Close the quilt to let the phone sleep)",
        PropertyList(),
        [this](const PropertyList&) -> ReturnValue {
            QuiltClose();
            return std::string("被子正在关闭");
        });

    ESP_LOGI(TAG, "Motor control initialized");
}

MotorControl::~MotorControl() {
    StopAll();
    if (limit_task_) {
        vTaskDelete(limit_task_);
    }
}

void MotorControl::InitLedc() {
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = MOTOR_RESOLUTION,
        .timer_num = MOTOR_TIMER,
        .freq_hz = MOTOR_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    for (int i = 0; i < 4; i++) {
        ledc_channel_config_t ch_cfg = {
            .gpio_num = motor_pins[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = motor_channels[i],
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = MOTOR_TIMER,
            .duty = 0,
            .hpoint = 0,
            .flags = { .output_invert = 0 }
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
    }
}

void MotorControl::InitLimitSwitches() {
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << MOTOR_LIMIT_HEAD_PIN) | (1ULL << MOTOR_LIMIT_TAIL_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_cfg);
}

void MotorControl::SetMotor(int index, int forward_duty, int reverse_duty) {
    int fwd_ch = index * 2;
    int rev_ch = index * 2 + 1;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, motor_channels[fwd_ch], forward_duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, motor_channels[fwd_ch]);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, motor_channels[rev_ch], reverse_duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, motor_channels[rev_ch]);
}

void MotorControl::QuiltOpen() {
    if (is_moving_) return;
    ESP_LOGI(TAG, "Opening quilt");
    is_moving_ = true;
    position_ = QuiltPosition::kMoving;
    target_direction_ = 1; // head

    // Motor0: reverse, Motor1: forward
    SetMotor(0, 0, kMotorSpeed);
    SetMotor(1, kMotorSpeed, 0);

    vTaskResume(limit_task_);
}

void MotorControl::QuiltClose() {
    if (is_moving_) return;
    ESP_LOGI(TAG, "Closing quilt");
    is_moving_ = true;
    position_ = QuiltPosition::kMoving;
    target_direction_ = -1; // tail

    // Motor0: forward, Motor1: reverse
    SetMotor(0, kMotorSpeed, 0);
    SetMotor(1, 0, kMotorSpeed);

    vTaskResume(limit_task_);
}

void MotorControl::StopAll() {
    SetMotor(0, 0, 0);
    SetMotor(1, 0, 0);
    is_moving_ = false;
}

void MotorControl::LimitMonitorTask(void* arg) {
    auto self = static_cast<MotorControl*>(arg);

    while (true) {
        int elapsed = 0;
        bool limit_reached = false;

        while (elapsed < kTimeoutMs) {
            if (self->target_direction_ == 1) {
                // Opening: check head limit
                if (gpio_get_level(MOTOR_LIMIT_HEAD_PIN) == 0) {
                    limit_reached = true;
                    self->position_ = QuiltPosition::kHead;
                    break;
                }
            } else if (self->target_direction_ == -1) {
                // Closing: check tail limit
                if (gpio_get_level(MOTOR_LIMIT_TAIL_PIN) == 0) {
                    limit_reached = true;
                    self->position_ = QuiltPosition::kTail;
                    break;
                }
            }

            vTaskDelay(pdMS_TO_TICKS(kPollIntervalMs));
            elapsed += kPollIntervalMs;
        }

        if (!limit_reached) {
            ESP_LOGW(TAG, "Motor timeout, stopping");
            self->position_ = QuiltPosition::kUnknown;
        }

        self->StopAll();
        ESP_LOGI(TAG, "Motors stopped, position=%d", (int)self->position_);

        // Suspend self until next motor operation
        vTaskSuspend(nullptr);
    }
}
