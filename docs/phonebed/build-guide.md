# 编译与烧录指南

## 环境要求

- **操作系统**: Windows 10/11
- **ESP-IDF**: v5.5.x（推荐v5.5.4）
- **Git**: 2.40+
- **纯英文路径**: 项目不能放在含中文的路径下

## 一、安装 ESP-IDF

1. 下载 https://dl.espressif.com/dl/esp-idf/
2. 选择 ESP-IDF v5.5.x Online Installer
3. 安装路径设为 `D:\Espressif`
4. 勾选 ESP32-S3 支持
5. 等待下载完成

验证：从开始菜单打开 **ESP-IDF 5.5 CMD**，运行：
```cmd
idf.py --version
```
应输出 `ESP-IDF v5.5.x`

## 二、克隆项目

```bash
cd /d D:\project1
git clone https://github.com/qll070226-a11y/phonebed-xiaozhi.git
cd phonebed-xiaozhi
git checkout phonebed-dev
```

## 三、编译（ESP-IDF CMD）

打开 **ESP-IDF 5.5 CMD**：

```cmd
cd /d D:\project1\phonebed-xiaozhi

:: 首次编译需要设置目标芯片
idf.py set-target esp32s3

:: 配置板型（二选一）：

:: 方法A：通过menuconfig图形界面
idf.py menuconfig
:: 进入 Xiaozhi Assistant → Board Type → PhoneBed ESP32-S3

:: 方法B：直接写sdkconfig（快速）
echo CONFIG_BOARD_TYPE_PHONEBED_ESP32_S3=y > sdkconfig
echo CONFIG_USE_DEVICE_AEC=y >> sdkconfig
echo CONFIG_LANGUAGE_ZH_CN=y >> sdkconfig

:: 编译
idf.py build
```

编译成功输出示例：
```
Project build complete. To flash, run:
 idf.py flash
```

## 四、烧录

用 USB 线连接开发板，确认 COM 端口号（设备管理器查看）：

```cmd
idf.py -p COM3 flash monitor
```

`monitor` 会显示串口日志，按 `Ctrl+]` 退出。

## 五、MSYS2/Git Bash 环境编译（Claude Code终端）

ESP-IDF 不原生支持 MSYS2，使用 `idf_run.py` 包装脚本：

```bash
IDF_PY="/d/Espressif/Espressif/python_env/idf5.5_py3.11_env/Scripts/python.exe D:/project1/phonebed-xiaozhi/idf_run.py"
cd /d/project1/phonebed-xiaozhi
$IDF_PY set-target esp32s3
$IDF_PY build
$IDF_PY -p COM3 flash monitor
```

## 六、常见问题

### Q: 编译报错 "中文路径"
A: 项目必须放在纯英文路径下，如 `D:\project1\`

### Q: 编译报错 "MSYSTEM" / "MSys/Mingw not supported"
A: 用 ESP-IDF CMD（开始菜单）编译，不要用 Git Bash / MSYS2

### Q: set-target 后 menuconfig 没有 PhoneBed 选项
A: 确认你在 `phonebed-dev` 分支上：`git checkout phonebed-dev`

### Q: 烧录后设备不工作
A: 检查：
1. COM端口是否正确
2. 开发板是否为 Waveshare ESP32-S3-Touch-LCD-3.49B-EN
3. 串口日志是否有报错（`idf.py monitor`）

### Q: 如何连接WiFi
A: 首次启动后，按 BOOT 键（GPIO 0）进入WiFi配网模式，用手机蓝牙配网

### Q: 如何注册小智AI
A: 访问 https://xiaozhi.me 注册账号，设备联网后自动绑定
