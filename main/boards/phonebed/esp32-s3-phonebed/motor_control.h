#pragma once

#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum class QuiltPosition {
    kHead,      // Open position
    kTail,      // Closed position
    kUnknown,
    kMoving
};

class MotorControl {
public:
    MotorControl();
    ~MotorControl();

    void QuiltOpen();
    void QuiltClose();
    void StopAll();
    bool IsMoving() const { return is_moving_; }
    QuiltPosition GetPosition() const { return position_; }

private:
    void InitLedc();
    void InitLimitSwitches();
    void SetMotor(int index, int forward_duty, int reverse_duty);
    static void LimitMonitorTask(void* arg);

    TaskHandle_t limit_task_ = nullptr;
    volatile bool is_moving_ = false;
    volatile QuiltPosition position_ = QuiltPosition::kUnknown;
    volatile int target_direction_ = 0; // 1=head(open), -1=tail(close)

    static constexpr int kMotorSpeed = 819; // ~80% of 1023
    static constexpr int kTimeoutMs = 5000;
    static constexpr int kPollIntervalMs = 50;
};
