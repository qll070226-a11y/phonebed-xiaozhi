#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include "lvgl.h"

// ===== Audio (I2S + Codec) =====
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_INPUT_REFERENCE    true

#define AUDIO_I2S_GPIO_MCLK  GPIO_NUM_7
#define AUDIO_I2S_GPIO_WS    GPIO_NUM_46
#define AUDIO_I2S_GPIO_BCLK  GPIO_NUM_15
#define AUDIO_I2S_GPIO_DIN   GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT  GPIO_NUM_45

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_NC
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_47
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_48
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR  ES7210_CODEC_DEFAULT_ADDR

// ===== Touch (I2C1) =====
#define Dev_Touch_I2C_SDA_PIN    GPIO_NUM_17
#define Dev_Touch_I2C_SCL_PIN    GPIO_NUM_18
#define I2C_Touch_ADDRESS        0x3b
#define I2C_Touch_SDA_PIN        GPIO_NUM_17
#define I2C_Touch_SCL_PIN        GPIO_NUM_18

// ===== Buttons =====
#define BOOT_BUTTON_GPIO    GPIO_NUM_0
#define PWR_BUTTON_GPIO     GPIO_NUM_16
#define BUTTON_LEFT_GPIO    GPIO_NUM_41
#define BUTTON_RIGHT_GPIO   GPIO_NUM_43

// ===== LCD (QSPI AXS15231B) =====
#define LCD_CS       GPIO_NUM_9
#define LCD_PCLK     GPIO_NUM_10
#define LCD_D0       GPIO_NUM_11
#define LCD_D1       GPIO_NUM_12
#define LCD_D2       GPIO_NUM_13
#define LCD_D3       GPIO_NUM_14
#define LCD_RST      GPIO_NUM_21
#define LCD_LIGHT    (-1)

#define DISPLAY_WIDTH   172
#define DISPLAY_HEIGHT  640
#define LVGL_DMA_BUFF_LEN    (DISPLAY_WIDTH * 64 * 2)
#define LVGL_SPIRAM_BUFF_LEN (DISPLAY_WIDTH * DISPLAY_HEIGHT * 2)

#define DISPLAY_ROTATION_90  false
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY  false
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

#define DISPLAY_BACKLIGHT_PIN            GPIO_NUM_8
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT  true

// ===== PhoneBed: Motor Control =====
#define MOTOR_PWM_PIN1       GPIO_NUM_2   // Motor0 forward
#define MOTOR_PWM_PIN2       GPIO_NUM_3   // Motor0 reverse
#define MOTOR_PWM_PIN3       GPIO_NUM_4   // Motor1 forward
#define MOTOR_PWM_PIN4       GPIO_NUM_5   // Motor1 reverse
#define MOTOR_LIMIT_HEAD_PIN GPIO_NUM_40  // Hall sensor - open position
#define MOTOR_LIMIT_TAIL_PIN GPIO_NUM_38  // Hall sensor - closed position

// ===== PhoneBed: Fan =====
#define FAN_PWM_PIN          GPIO_NUM_1

// ===== PhoneBed: Charge Detection =====
#define CHARGE_DETECT_PIN    GPIO_NUM_44

// ===== PhoneBed: RTC =====
#define RTC_PCF85063_ADDR    0x51

#endif // _BOARD_CONFIG_H_
