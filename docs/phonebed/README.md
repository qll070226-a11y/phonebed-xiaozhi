# PhoneBed + XiaoZhi AI 智能手机床项目

> 基于 ESP32-S3 的智能手机床，融合何同学开源手机床硬件 + 小智AI语音助手
> 参赛：全国大学生嵌入式芯片与系统设计竞赛 & 全国大学生物联网设计竞赛（乐鑫赛题）

## 一、项目简介

将手机放入手机床，自动"盖好被子"并充电；设定闹钟后，到点自动"叫醒"手机。同时集成小智AI语音助手，支持语音控制所有功能：

- "小智，帮我设个明天7点的闹钟"
- "小智，把被子打开"
- "小智，把风扇调大一点"

### 核心亮点

| 特性 | 说明 |
|------|------|
| AI语音交互 | 小智AI大模型对话，语音控制手机床 |
| 自动盖被 | 手机放上自动检测充电，电机驱动盖被子 |
| 智能闹钟 | RTC + SNTP双重保障，语音/按键设定 |
| 物联网 | WiFi联网，云端大模型，MCP协议扩展 |
| 全开源 | 硬件PCB + 3D打印 + 固件代码 |

## 二、硬件平台

### 主控
- **芯片**: ESP32-S3 (Xtensa LX7 双核 240MHz, WiFi+BLE)
- **开发板**: [Waveshare ESP32-S3-Touch-LCD-3.49B-EN](https://www.waveshare.net/shop/ESP32-S3-Touch-LCD-3.49B-EN.htm)
- **屏幕**: 3.49寸触摸LCD (AXS15231B, 172x640, QSPI)
- **音频**: ES8311编解码器 (I2S, 麦克风+喇叭)

### 手机床硬件
- 2x GA12-N20 减速电机（盖被子机构）
- 2x 9015散热风扇
- 霍尔传感器（限位检测）
- 无线充电模块
- PCF85063 RTC时钟芯片
- 3D打印外壳

### 物料采购参考
详见 [HTX手机床开源制作说明文档](https://oshwhub.com/htx-studio/project_kpilekig)

## 三、项目结构

```
phonebed-xiaozhi/                    # 基于 xiaozhi-esp32
├── main/
│   ├── boards/
│   │   ├── phonebed/                # ★ 我们的自定义板
│   │   │   └── esp32-s3-phonebed/
│   │   │       ├── config.h         # GPIO引脚定义
│   │   │       ├── config.json      # 构建配置
│   │   │       ├── phonebed_board.cc    # 主板类（核心）
│   │   │       ├── phonebed_backlight.h/cc  # LCD背光
│   │   │       ├── motor_control.h/cc   # 电机控制+MCP工具
│   │   │       ├── fan_control.h/cc     # 风扇控制+MCP工具
│   │   │       ├── alarm_clock.h/cc     # 闹钟+MCP工具
│   │   │       └── custom_lcd_display.h/cc  # LCD驱动
│   │   ├── waveshare/              # 微雪原版板（参考）
│   │   └── common/                 # 公共基类
│   ├── audio/                      # 小智音频管线
│   ├── display/                    # 显示驱动
│   ├── protocols/                  # WebSocket/MQTT通信
│   ├── mcp_server.h/cc            # MCP工具服务
│   └── application.cc             # 主应用逻辑
├── docs/
│   ├── phonebed/                   # ★ 项目文档（你在这里）
│   │   ├── README.md               # 本文件
│   │   ├── pin-map.md              # 引脚规划
│   │   ├── build-guide.md          # 编译指南
│   │   └── competition-info.md     # 比赛信息
│   ├── custom-board.md            # 小智自定义板指南
│   └── websocket.md               # 通信协议文档
├── sdkconfig.defaults             # ESP-IDF默认配置
├── sdkconfig.defaults.esp32s3    # ESP32-S3专用配置
└── partitions/                    # Flash分区表
```

## 四、开发环境搭建

### 4.1 安装 ESP-IDF

1. 下载 [ESP-IDF Windows Installer](https://dl.espressif.com/dl/esp-idf/)
2. 安装 ESP-IDF v5.5.x 到 `D:\Espressif`（避免中文路径）
3. 安装完成后从开始菜单打开 **ESP-IDF 5.5 CMD**

### 4.2 克隆项目

```bash
git clone https://github.com/qll070226-a11y/phonebed-xiaozhi.git
cd phonebed-xiaozhi
git checkout phonebed-dev
```

> 注意：项目必须放在**纯英文路径**下，ESP-IDF不支持中文路径

### 4.3 编译

**方法一：ESP-IDF CMD（推荐）**

从开始菜单打开 "ESP-IDF 5.5 CMD"：
```cmd
cd /d D:\project1\phonebed-xiaozhi\xiaozhi-esp32
idf.py set-target esp32s3
idf.py menuconfig
:: 进入 Xiaozhi Assistant → Board Type → 选择 "PhoneBed ESP32-S3"
idf.py build
```

**方法二：使用 idf_run.py 包装脚本（MSYS2/Git Bash环境）**

```bash
IDF_PY="/d/Espressif/Espressif/python_env/idf5.5_py3.11_env/Scripts/python.exe D:/project1/phonebed-xiaozhi/idf_run.py"
cd /d/project1/phonebed-xiaozhi/xiaozhi-esp32
$IDF_PY set-target esp32s3
$IDF_PY build
```

### 4.4 烧录

```cmd
idf.py -p COM3 flash monitor
:: 替换 COM3 为你的实际串口号
```

### 4.5 快速配置（不用menuconfig）

在项目根目录创建 `sdkconfig` 文件写入：
```
CONFIG_BOARD_TYPE_PHONEBED_ESP32_S3=y
CONFIG_USE_DEVICE_AEC=y
CONFIG_LANGUAGE_ZH_CN=y
```

## 五、GPIO引脚分配

| GPIO | 功能 | 模块 |
|------|------|------|
| **LCD (QSPI SPI3)** | | |
| 8 | 背光PWM | LEDC TIMER_1 CH_5 |
| 9 | CS | SPI3 |
| 10 | PCLK | SPI3 |
| 11-14 | D0-D3 | SPI3 QSPI |
| 21 | RST | GPIO |
| **触摸 (I2C1)** | | |
| 17 | SDA | I2C1, 地址0x3B |
| 18 | SCL | I2C1 |
| **音频 (I2S)** | | |
| 6 | DIN (麦克风) | I2S |
| 7 | MCLK | I2S |
| 15 | BCLK | I2S |
| 45 | DOUT (喇叭) | I2S |
| 46 | WS | I2S |
| **I2C0 总线** | | |
| 47 | SDA | RTC+IMU+TCA9554+ES8311 |
| 48 | SCL | |
| **电机** | | |
| 2 | 电机0正转 | LEDC TIMER_2 CH_0 |
| 3 | 电机0反转 | LEDC TIMER_2 CH_1 |
| 4 | 电机1正转 | LEDC TIMER_2 CH_2 |
| 5 | 电机1反转 | LEDC TIMER_2 CH_3 |
| 40 | 限位-头(开) | 霍尔传感器 |
| 38 | 限位-尾(合) | 霍尔传感器 |
| **风扇** | | |
| 1 | PWM | LEDC TIMER_3 CH_4 |
| **按键** | | |
| 0 | BOOT键 | 语音对话触发 |
| 16 | 电源键 | 板载 |
| 41 | 左键 | 闹钟操作 |
| 43 | 右键 | 被子开合 |
| **充电检测** | | |
| 44 | 充电检测 | 下降沿中断 |

### LEDC通道分配

| 定时器 | 通道 | GPIO | 频率 | 用途 |
|--------|------|------|------|------|
| TIMER_1 | CH_5 | 8 | 25kHz | LCD背光 |
| TIMER_2 | CH_0-3 | 2-5 | 1kHz | 4路电机PWM |
| TIMER_3 | CH_4 | 1 | 10kHz | 风扇 |

## 六、MCP工具（AI语音可调用）

| 工具名 | 功能 | 参数 |
|--------|------|------|
| `self.quilt.open` | 打开被子叫醒手机 | 无 |
| `self.quilt.close` | 盖好被子让手机睡觉 | 无 |
| `self.fan.set_speed` | 设置风扇速度 | level: 0-4 (0=关) |
| `self.alarm.set` | 设置闹钟 | hour: 0-23, minute: 0-59, enabled: bool |
| `self.alarm.get` | 查询闹钟和当前时间 | 无 |

用户通过语音对话触发，大模型自动调用对应工具。

## 七、代码模块说明

### phonebed_board.cc（主板类）
- 继承 `WifiBoard`，拥有小智所有功能（WiFi、语音、显示）
- 初始化所有硬件：I2C、SPI、LCD、Touch、音频编解码器
- 创建 MotorControl、FanControl、AlarmClock 实例
- 充电检测ISR → task notification → 安全执行盖被子+开风扇

### motor_control.cc（电机控制）
- LEDC PWM 驱动2个N20电机（4通道H桥）
- 霍尔传感器限位检测（FreeRTOS任务，50ms轮询）
- 5秒超时安全保护
- 注册MCP工具：`self.quilt.open`, `self.quilt.close`

### fan_control.cc（风扇控制）
- LEDC PWM 5档调速（反向占空比：0=全速，1023=关）
- 注册MCP工具：`self.fan.set_speed`

### alarm_clock.cc（闹钟）
- PCF85063 RTC驱动（I2C，BCD编码）
- SNTP网络时间同步到RTC
- 1秒定时器检查闹钟匹配
- 触发动作：打开被子 + 低速风扇
- NVS持久化闹钟设置
- 注册MCP工具：`self.alarm.set`, `self.alarm.get`

### phonebed_backlight.cc（自定义背光）
- 使用LEDC TIMER_1 + CH_5（避免与电机TIMER_2 CH_0-3冲突）

## 八、比赛信息

### 嵌入式芯片与系统设计竞赛（嵌赛）
- **官网**: https://www.socchina.net
- **报名截止**: 2026-04-20
- **作品提交**: 2026年7月上旬
- **乐鑫赛题**: 选题五 — 智能交互驱动的AIoT应用系统
- **要求**: 自制PCB，丝印 "AI for Design, Design for AI"
- **乐鑫页面**: https://www.espressif.com/zh-hans/ecosystem/education/competition/socchina

### 物联网设计竞赛
- **官网**: http://iot.sjtu.edu.cn
- **报名截止**: 2026-06-15
- **作品提交**: 2026-07-27
- **乐鑫命题**: ESP32-S3/C5/P4 三选一
- **乐鑫页面**: https://www.espressif.com/zh-hans/ecosystem/education/competition/iot

### 开发板获取
- 乐鑫淘宝店购买，提交有效作品后可获 99元代金券返还
- 店铺：乐鑫科技 Espressif Online

## 九、开源资源

| 资源 | 链接 |
|------|------|
| 本项目 | https://github.com/qll070226-a11y/phonebed-xiaozhi |
| 小智AI原版 | https://github.com/78/xiaozhi-esp32 |
| 手机床PCB+原理图 | https://oshwhub.com/htx-studio/project_kpilekig |
| 手机床GitHub | https://github.com/htx-studio/PhoneBed |
| 3D打印模型 | https://makerworld.com.cn/zh/models/2327961 |
| 何同学视频 | https://www.bilibili.com/video/BV1LQwkzvEtZ |
| 乐鑫赛题指南PDF | 见仓库 docs/ 目录 |
| ESP-IDF文档 | https://docs.espressif.com/projects/esp-idf/zh_CN/v5.5.4/ |

## 十、开发分工建议

| 模块 | 工作内容 | 难度 |
|------|----------|------|
| 自制PCB | 基于立创开源原理图，集成麦克风+功放，丝印赛事LOGO | 中 |
| 3D打印外壳 | 参考何同学开源模型，可适当改造 | 低 |
| 固件开发 | 在 phonebed-dev 分支上迭代功能 | 高 |
| UI界面 | 手机床专属LVGL界面（时钟、闹钟、状态） | 中 |
| 设计报告 | 参赛文档、演示视频 | 中 |

## 十一、Git 工作流

```bash
# 日常开发
git checkout phonebed-dev
# 编辑代码...
git add -A
git commit -m "feat: 你的改动描述"
git push

# 同步上游小智更新（如需要）
git fetch upstream
git merge upstream/main
```
