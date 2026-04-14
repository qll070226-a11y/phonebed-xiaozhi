#pragma once

#include <cstdint>

class FanControl {
public:
    FanControl();
    void SetSpeed(int level); // 0=off, 1=low, 2=med-low, 3=med-high, 4=high
    int GetSpeed() const { return speed_level_; }

private:
    void InitLedc();
    int speed_level_ = 0;
};
