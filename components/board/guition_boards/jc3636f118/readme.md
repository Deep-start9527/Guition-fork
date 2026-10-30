# JC3636F118

ESP32-S31，360×360 ST77916 QSPI 屏幕，CST816S 电容触摸。默认启用电容触摸，可在 menuconfig 选择 `No touch`；无电阻触摸配置。

| 外设 | GPIO |
| --- | --- |
| LCD SCK / CS | 12 / 13 |
| LCD DATA0 / DATA1 / DATA2 / DATA3 | 14 / 15 / 16 / 17 |
| LCD RESET / 背光 | 18 / 5 |
| 触摸 SCL / SDA / RESET / INT | 10 / 11 / 9 / 8 |
| SDMMC CLK / CMD / D0 / D1 / D2 / D3 | 24 / 25 / 20 / 21 / 22 / 23 |
| PDM 输入 CLK / DATA | 39 / 37 |
| I2S 输出 BCLK / LRCK / DATA | 53 / 51 / 52 |

LCD TE 接线为 GPIO19，默认不启用 TE 同步。SDMMC 使用 4 位总线，未配置片内 LDO 电源控制。

音频无外部 codec，使用 `chip: internal`：`audio_adc` 连接 I2S0 的 `i2s_audio_in`，以 PDM 输入并转换为 PCM；`audio_dac` 连接 I2S1 的 `i2s_audio_out`，使用标准 Philips I2S 输出，无 MCLK 或 I2C 控制。输出接口使用软件 dummy codec。

输入默认 16 kHz、16 位、双声道，采集同一 PDM 数据线的左右时隙；有效麦克风时隙取决于硬件 L/R 接线，应用按实际接线选取。输出默认 48 kHz、16 位、双声道。应用按需通过 ESP Board Manager 初始化音频设备并获取句柄，调用 `esp_codec_dev_open()` 配置音频流，再使用 `esp_codec_dev_read()` / `esp_codec_dev_write()` 读写 PCM 数据。

CPU 默认 320 MHz，Flash 默认 16 MB、QIO 80 MHz，八线 PSRAM 默认 200 MHz。编码器、IMU 及电池 ADC 引脚定义位于 `board_pins.h`，不自动初始化这些外设。
