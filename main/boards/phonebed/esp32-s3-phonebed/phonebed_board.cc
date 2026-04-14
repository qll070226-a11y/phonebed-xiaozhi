#include "wifi_board.h"
#include "codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"

#include "phonebed_backlight.h"
#include "motor_control.h"
#include "fan_control.h"
#include "alarm_clock.h"
#include "custom_lcd_display.h"

#include <esp_log.h>
#include "i2c_device.h"
#include <driver/i2c_master.h>
#include <driver/ledc.h>
#include <driver/gpio.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>
#include "esp_io_expander_tca9554.h"
#include "esp_lcd_axs15231b.h"
#include <lvgl.h>

#define TAG "PhoneBedBoard"

static const axs15231b_lcd_init_cmd_t lcd_init_cmds[] = {
    {0x11, (uint8_t []){0x00}, 0, 100},
    {0x29, (uint8_t []){0x00}, 0, 100},
};

class PhoneBedBoard : public WifiBoard {
private:
    Button boot_button_;
    Button pwr_button_;
    Button left_button_;
    Button right_button_;
    i2c_master_bus_handle_t i2c_bus_;
    esp_io_expander_handle_t io_expander_ = NULL;
    LcdDisplay* display_ = nullptr;
    i2c_master_dev_handle_t disp_touch_dev_handle_ = NULL;
    lv_indev_t* touch_indev_ = NULL;
    bool is_pwr_control_en_ = false;

    // PhoneBed modules
    MotorControl* motor_control_ = nullptr;
    FanControl* fan_control_ = nullptr;
    AlarmClock* alarm_clock_ = nullptr;
    TaskHandle_t charge_task_ = nullptr;

    // ===== Initialization (same as waveshare 3.49) =====

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = { .enable_internal_pullup = 1 },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeTca9554() {
        esp_err_t ret = esp_io_expander_new_i2c_tca9554(i2c_bus_,
            ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &io_expander_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "TCA9554 init failed");
            return;
        }
        ret = esp_io_expander_set_dir(io_expander_,
            IO_EXPANDER_PIN_NUM_7 | IO_EXPANDER_PIN_NUM_6, IO_EXPANDER_OUTPUT);
        ESP_ERROR_CHECK(ret);
        vTaskDelay(pdMS_TO_TICKS(100));
        ret = esp_io_expander_set_level(io_expander_,
            IO_EXPANDER_PIN_NUM_7 | IO_EXPANDER_PIN_NUM_6, 1);
        ESP_ERROR_CHECK(ret);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.data0_io_num = LCD_D0;
        buscfg.data1_io_num = LCD_D1;
        buscfg.data2_io_num = LCD_D2;
        buscfg.data3_io_num = LCD_D3;
        buscfg.sclk_io_num = LCD_PCLK;
        buscfg.max_transfer_sz = LVGL_DMA_BUFF_LEN;
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        gpio_config_t gpio_conf = {};
        gpio_conf.intr_type = GPIO_INTR_DISABLE;
        gpio_conf.mode = GPIO_MODE_OUTPUT;
        gpio_conf.pin_bit_mask = ((uint64_t)0x01 << LCD_RST);
        gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

        esp_lcd_panel_io_spi_config_t io_config = AXS15231B_PANEL_IO_QSPI_CONFIG(LCD_CS, NULL, NULL);
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        const axs15231b_vendor_config_t vendor_config = {
            .init_cmds = lcd_init_cmds,
            .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
            .flags = { .use_qspi_interface = 1 },
        };
        esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = -1,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = 16,
            .vendor_config = (void*)&vendor_config,
        };
        esp_lcd_new_panel_axs15231b(panel_io, &panel_config, &panel);

        gpio_set_level(LCD_RST, 1);
        vTaskDelay(pdMS_TO_TICKS(30));
        gpio_set_level(LCD_RST, 0);
        vTaskDelay(pdMS_TO_TICKS(250));
        gpio_set_level(LCD_RST, 1);
        vTaskDelay(pdMS_TO_TICKS(30));
        esp_lcd_panel_init(panel);

        display_ = new CustomLcdDisplay(panel_io, panel,
            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTouch() {
        i2c_master_bus_handle_t touch_i2c_bus;
        i2c_master_bus_config_t i2c_bus_cfg = {};
        i2c_bus_cfg.i2c_port = (i2c_port_t)I2C_NUM_1;
        i2c_bus_cfg.sda_io_num = I2C_Touch_SDA_PIN;
        i2c_bus_cfg.scl_io_num = I2C_Touch_SCL_PIN;
        i2c_bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
        i2c_bus_cfg.glitch_ignore_cnt = 7;
        i2c_bus_cfg.intr_priority = 0;
        i2c_bus_cfg.trans_queue_depth = 0;
        i2c_bus_cfg.flags.enable_internal_pullup = 1;
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &touch_i2c_bus));

        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = I2C_Touch_ADDRESS,
            .scl_speed_hz = 300000,
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(touch_i2c_bus, &dev_cfg, &disp_touch_dev_handle_));

        touch_indev_ = lv_indev_create();
        lv_indev_set_type(touch_indev_, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(touch_indev_, TouchInputReadCallback);
        lv_indev_set_user_data(touch_indev_, disp_touch_dev_handle_);
    }

    static void TouchInputReadCallback(lv_indev_t* indev, lv_indev_data_t* data) {
        auto i2c_dev = (i2c_master_dev_handle_t)lv_indev_get_user_data(indev);
        uint8_t cmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0, 0, 0, 0x0e, 0, 0, 0};
        uint8_t buf[32] = {};
        i2c_master_transmit_receive(i2c_dev, cmd, 11, buf, 32, 1000);

        uint16_t x = (((uint16_t)buf[2] & 0x0f) << 8) | (uint16_t)buf[3];
        uint16_t y = (((uint16_t)buf[4] & 0x0f) << 8) | (uint16_t)buf[5];

        if (buf[1] > 0 && buf[1] < 5) {
            data->state = LV_INDEV_STATE_PRESSED;
            if (x > DISPLAY_WIDTH) x = DISPLAY_WIDTH;
            if (y > DISPLAY_HEIGHT) y = DISPLAY_HEIGHT;
            data->point.x = y;
            data->point.y = (DISPLAY_HEIGHT - x);
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    }

    // ===== Buttons =====

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        pwr_button_.OnLongPress([this]() {
            if (is_pwr_control_en_) {
                is_pwr_control_en_ = false;
                esp_io_expander_set_level(io_expander_, IO_EXPANDER_PIN_NUM_6, 0);
            }
        });

        pwr_button_.OnPressUp([this]() {
            if (!is_pwr_control_en_) {
                is_pwr_control_en_ = true;
            }
        });

        // Left button: toggle alarm enable/disable on long press
        left_button_.OnLongPress([this]() {
            if (alarm_clock_) {
                auto alarm = alarm_clock_->GetAlarm();
                alarm_clock_->SetAlarm(alarm.hour, alarm.minute, !alarm.enabled);
                ESP_LOGI(TAG, "Alarm toggled: %s", alarm.enabled ? "OFF" : "ON");
            }
        });

        // Right button: stop alarm / open quilt manually
        right_button_.OnClick([this]() {
            if (motor_control_ && !motor_control_->IsMoving()) {
                auto pos = motor_control_->GetPosition();
                if (pos == QuiltPosition::kTail || pos == QuiltPosition::kUnknown) {
                    motor_control_->QuiltOpen();
                } else {
                    motor_control_->QuiltClose();
                }
            }
        });
    }

    // ===== PhoneBed Features =====

    void InitializePhoneBed() {
        motor_control_ = new MotorControl();
        fan_control_ = new FanControl();
        alarm_clock_ = new AlarmClock(i2c_bus_, motor_control_, fan_control_);

        // Charge detection: GPIO 44, falling edge
        InitChargeDetection();

        ESP_LOGI(TAG, "PhoneBed features initialized");
    }

    void InitChargeDetection() {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << CHARGE_DETECT_PIN),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_NEGEDGE,
        };
        gpio_config(&io_conf);

        // Create task for deferred charge handling
        xTaskCreatePinnedToCore(ChargeHandlerTask, "charge", 2048, this, 5, &charge_task_, 1);

        gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
        gpio_isr_handler_add(CHARGE_DETECT_PIN, ChargeIsrHandler, this);
    }

    static void IRAM_ATTR ChargeIsrHandler(void* arg) {
        auto self = static_cast<PhoneBedBoard*>(arg);
        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveFromISR(self->charge_task_, &woken);
        portYIELD_FROM_ISR(woken);
    }

    static void ChargeHandlerTask(void* arg) {
        auto self = static_cast<PhoneBedBoard*>(arg);
        while (true) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            ESP_LOGI(TAG, "Phone placed on charger, closing quilt");
            if (self->motor_control_) {
                self->motor_control_->QuiltClose();
            }
            if (self->fan_control_) {
                self->fan_control_->SetSpeed(3); // Medium-high for charging
            }
        }
    }

    void GetPwrCurrentState() {
        if (gpio_get_level(PWR_BUTTON_GPIO)) {
            is_pwr_control_en_ = true;
        }
    }

public:
    PhoneBedBoard()
        : boot_button_(BOOT_BUTTON_GPIO)
        , pwr_button_(PWR_BUTTON_GPIO)
        , left_button_(BUTTON_LEFT_GPIO)
        , right_button_(BUTTON_RIGHT_GPIO)
    {
        InitializeI2c();
        InitializeTca9554();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeTouch();
        GetPwrCurrentState();
        GetBacklight()->RestoreBrightness();

        // PhoneBed-specific initialization
        InitializePhoneBed();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR,
            AUDIO_CODEC_ES7210_ADDR,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PhonebedBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(PhoneBedBoard);
