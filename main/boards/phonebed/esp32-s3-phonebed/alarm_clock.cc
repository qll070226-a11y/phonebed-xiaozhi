#include "alarm_clock.h"
#include "motor_control.h"
#include "fan_control.h"
#include "config.h"
#include "mcp_server.h"
#include "settings.h"

#include <esp_log.h>
#include <esp_sntp.h>
#include <ctime>
#include <cstring>

#define TAG "AlarmClock"

// PCF85063 registers
#define PCF85063_REG_CTRL1    0x00
#define PCF85063_REG_SECONDS  0x04
#define PCF85063_REG_MINUTES  0x05
#define PCF85063_REG_HOURS    0x06
#define PCF85063_REG_DAYS     0x07
#define PCF85063_REG_WEEKDAYS 0x08
#define PCF85063_REG_MONTHS   0x09
#define PCF85063_REG_YEARS    0x0A

AlarmClock::AlarmClock(i2c_master_bus_handle_t i2c_bus, MotorControl* motor, FanControl* fan)
    : i2c_bus_(i2c_bus), motor_(motor), fan_(fan) {
    InitRtc();

    // Load alarm from NVS
    Settings settings("alarm");
    alarm_.hour = settings.GetInt("hour", 7);
    alarm_.minute = settings.GetInt("minute", 0);
    alarm_.enabled = settings.GetInt("enabled", 0) != 0;

    // Create 1-second timer to check alarm
    esp_timer_create_args_t timer_args = {
        .callback = CheckAlarmCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "alarm_check",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &check_timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(check_timer_, 1000000)); // 1 second

    RegisterMcpTools();
    ESP_LOGI(TAG, "Alarm clock initialized, alarm=%02d:%02d enabled=%d",
             alarm_.hour, alarm_.minute, alarm_.enabled);
}

void AlarmClock::InitRtc() {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = RTC_PCF85063_ADDR,
        .scl_speed_hz = 400000,
    };
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &rtc_dev_);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "PCF85063 RTC not found, using system time only");
        rtc_dev_ = nullptr;
    }
}

void AlarmClock::WriteReg(uint8_t reg, uint8_t val) {
    if (!rtc_dev_) return;
    uint8_t buf[2] = {reg, val};
    i2c_master_transmit(rtc_dev_, buf, 2, 100);
}

uint8_t AlarmClock::ReadReg(uint8_t reg) {
    if (!rtc_dev_) return 0;
    uint8_t val = 0;
    i2c_master_transmit_receive(rtc_dev_, &reg, 1, &val, 1, 100);
    return val;
}

RtcTime AlarmClock::ReadTime() {
    RtcTime t = {};
    if (rtc_dev_) {
        // Read from RTC
        t.second  = Bcd2Dec(ReadReg(PCF85063_REG_SECONDS) & 0x7F);
        t.minute  = Bcd2Dec(ReadReg(PCF85063_REG_MINUTES) & 0x7F);
        t.hour    = Bcd2Dec(ReadReg(PCF85063_REG_HOURS) & 0x3F);
        t.day     = Bcd2Dec(ReadReg(PCF85063_REG_DAYS) & 0x3F);
        t.weekday = ReadReg(PCF85063_REG_WEEKDAYS) & 0x07;
        t.month   = Bcd2Dec(ReadReg(PCF85063_REG_MONTHS) & 0x1F);
        t.year    = Bcd2Dec(ReadReg(PCF85063_REG_YEARS)) + 2000;
    } else {
        // Fallback: use system time (SNTP synced)
        time_t now;
        time(&now);
        struct tm* tm_info = localtime(&now);
        if (tm_info) {
            t.year = tm_info->tm_year + 1900;
            t.month = tm_info->tm_mon + 1;
            t.day = tm_info->tm_mday;
            t.hour = tm_info->tm_hour;
            t.minute = tm_info->tm_min;
            t.second = tm_info->tm_sec;
            t.weekday = tm_info->tm_wday;
        }
    }
    return t;
}

void AlarmClock::SyncFromSntp() {
    if (!rtc_dev_) return;

    time_t now;
    time(&now);
    struct tm* tm_info = localtime(&now);
    if (!tm_info || tm_info->tm_year < 120) return; // Not synced yet

    WriteReg(PCF85063_REG_SECONDS,  Dec2Bcd(tm_info->tm_sec));
    WriteReg(PCF85063_REG_MINUTES,  Dec2Bcd(tm_info->tm_min));
    WriteReg(PCF85063_REG_HOURS,    Dec2Bcd(tm_info->tm_hour));
    WriteReg(PCF85063_REG_DAYS,     Dec2Bcd(tm_info->tm_mday));
    WriteReg(PCF85063_REG_WEEKDAYS, tm_info->tm_wday);
    WriteReg(PCF85063_REG_MONTHS,   Dec2Bcd(tm_info->tm_mon + 1));
    WriteReg(PCF85063_REG_YEARS,    Dec2Bcd(tm_info->tm_year % 100));

    ESP_LOGI(TAG, "RTC synced from SNTP: %04d-%02d-%02d %02d:%02d:%02d",
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
}

void AlarmClock::SetAlarm(int hour, int minute, bool enabled) {
    alarm_.hour = hour;
    alarm_.minute = minute;
    alarm_.enabled = enabled;
    alarm_triggered_ = false;

    // Persist to NVS
    Settings settings("alarm", true);
    settings.SetInt("hour", hour);
    settings.SetInt("minute", minute);
    settings.SetInt("enabled", enabled ? 1 : 0);

    ESP_LOGI(TAG, "Alarm set to %02d:%02d, enabled=%d", hour, minute, enabled);
}

void AlarmClock::CheckAlarmCallback(void* arg) {
    auto self = static_cast<AlarmClock*>(arg);
    if (!self->alarm_.enabled) return;

    RtcTime now = self->ReadTime();

    if (now.hour == self->alarm_.hour &&
        now.minute == self->alarm_.minute &&
        now.second == 0) {
        if (!self->alarm_triggered_) {
            self->alarm_triggered_ = true;
            ESP_LOGI(TAG, "ALARM TRIGGERED! %02d:%02d", now.hour, now.minute);

            // Open quilt to wake up the phone
            if (self->motor_) {
                self->motor_->QuiltOpen();
            }
            // Set fan to low
            if (self->fan_) {
                self->fan_->SetSpeed(1);
            }
        }
    } else {
        // Reset trigger flag when not at alarm time
        self->alarm_triggered_ = false;
    }
}

void AlarmClock::RegisterMcpTools() {
    auto& mcp = McpServer::GetInstance();

    mcp.AddTool("self.alarm.set",
        "设置闹钟时间 (Set alarm: hour 0-23, minute 0-59, enabled true/false)",
        PropertyList({
            Property("hour", kPropertyTypeInteger, 0, 23),
            Property("minute", kPropertyTypeInteger, 0, 59),
            Property("enabled", kPropertyTypeBoolean)
        }),
        [this](const PropertyList& props) -> ReturnValue {
            int hour = props["hour"].value<int>();
            int minute = props["minute"].value<int>();
            bool enabled = props["enabled"].value<bool>();
            SetAlarm(hour, minute, enabled);
            if (enabled) {
                return std::string("闹钟已设置为 " + std::to_string(hour) + ":" +
                                   (minute < 10 ? "0" : "") + std::to_string(minute));
            } else {
                return std::string("闹钟已关闭");
            }
        });

    mcp.AddTool("self.alarm.get",
        "查询当前闹钟设置 (Get current alarm settings)",
        PropertyList(),
        [this](const PropertyList&) -> ReturnValue {
            RtcTime now = ReadTime();
            char buf[128];
            snprintf(buf, sizeof(buf),
                     "当前时间: %02d:%02d:%02d, 闹钟: %02d:%02d, %s",
                     now.hour, now.minute, now.second,
                     alarm_.hour, alarm_.minute,
                     alarm_.enabled ? "已开启" : "已关闭");
            return std::string(buf);
        });
}
