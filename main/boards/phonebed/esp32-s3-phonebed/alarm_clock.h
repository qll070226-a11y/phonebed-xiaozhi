#pragma once

#include <cstdint>
#include <driver/i2c_master.h>
#include <esp_timer.h>

class MotorControl;
class FanControl;

struct AlarmTime {
    int hour = 7;
    int minute = 0;
    bool enabled = false;
};

struct RtcTime {
    int year, month, day, hour, minute, second, weekday;
};

class AlarmClock {
public:
    AlarmClock(i2c_master_bus_handle_t i2c_bus, MotorControl* motor, FanControl* fan);

    void SetAlarm(int hour, int minute, bool enabled);
    AlarmTime GetAlarm() const { return alarm_; }
    RtcTime ReadTime();
    void SyncFromSntp();

private:
    void InitRtc();
    void RegisterMcpTools();
    static void CheckAlarmCallback(void* arg);

    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    i2c_master_dev_handle_t rtc_dev_ = nullptr;
    esp_timer_handle_t check_timer_ = nullptr;
    AlarmTime alarm_;
    MotorControl* motor_;
    FanControl* fan_;
    bool alarm_triggered_ = false;

    // PCF85063 helpers
    uint8_t Bcd2Dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
    uint8_t Dec2Bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }
    void WriteReg(uint8_t reg, uint8_t val);
    uint8_t ReadReg(uint8_t reg);
};
