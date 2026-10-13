# guition_boards

Guition 开发板的 ESP Board Manager 板库，提供显示、触摸、背光及可选 SD 卡配置。
组件名为 `guition-jc/guition_boards`，当前版本 `0.1.0`，支持 ESP-IDF 6.1.x。
保留 21 个已迁移板型及 2 个历史 ESP32-S31 板型；S31 不在本版本的编译支持范围内。
原有硬件参数和公共 `guition_*` C API 保持不变。

## 安装和依赖

将本目录复制到应用的 `components/guition_boards`，或在板库发布后执行：

```bash
idf.py add-dependency "guition-jc/guition_boards==0.1.0"
```

固定基础依赖为 `espressif/esp_board_manager ==0.7.3~1`、
`espressif/esp_lcd_touch ==1.2.1`。板型驱动由 BMGR 按所选板型生成依赖。
JC8012 使用已发布的 `guition-jc/esp_lcd_touch_gsl3680 ==0.1.0`，直接从组件中心下载。

`guition-jc/esp_lcd_axs15260 ==1.0.0` 也已发布。当前板型未调用 AXS15260，
所以它不是板库的全局依赖；需要该控制器的应用可执行：

```bash
idf.py add-dependency "guition-jc/esp_lcd_axs15260==1.0.0"
```

公共适配不依赖 Arduino 或 LVGL。组件 CMake 仅编译公共源码；BMGR 的
`gen_bmgr_codes` 选择并编译当前板型的工厂函数，不能递归编译所有板型。
`board_catalog.json` 记录 21 个迁移型号及其 BMGR 标识，JC1060 的两个外壳版本分别配置。

## 最小示例

`examples/board_init` 通过相对 `override_path` 使用本组件，可在发布前直接构建。
在已激活的 ESP-IDF 6.1 环境中执行：

```bash
cd examples/board_init
idf.py set-target esp32p4
idf.py bmgr -b jc8012p4a1
idf.py build
```

复制示例到独立应用时，将 `main/idf_component.yml` 的 `override_path` 改为本地
板库路径；板库发布后也可删除该字段使用组件中心版本。GSL3680 无需本地覆盖。
更换板型时使用对应芯片目标并重新生成 BMGR 配置。编译通过不等同于实机验证。

## Kconfig 与板型迁移接入

公共 `Kconfig` 提供 `GUITION_TOUCH_CAPACITIVE`、`GUITION_TOUCH_RESISTIVE` 和
`GUITION_TOUCH_NONE`。每块板的默认配置和可选类型来自 Arduino 的有效 C/R/N 分支。

`profiles/<board>.Kconfig` 保存板型能力定义。迁移某块板时，将其内容并入该板的
`Kconfig.projbuild`。BMGR 生成代码时会将**当前板型**的配置并入生成组件。
未接入该能力文件的旧板型不会出现新触摸菜单，也不会改变其原有初始化路径。
目标芯片不匹配时适配器关闭。

这样无需在公共 Kconfig 中引用消费工程未定义的其他型号符号。
同一芯片上的板型切换使用 `idf.py bmgr -b <board>` 重新生成；切换芯片时使用
隔离工程/构建目录，不能仅切换触摸选项代替板型生成。

## 已迁移的 ESP32/C3 板型

| board | 屏幕 | 默认触摸 | 可选触摸 |
| --- | --- | --- | --- |
| esp32_2424s012 | GC9A01，240×240 | CST816S | 电容 / 无 |
| esp32_2432s024 | ST7789，240×320 | XPT2046 | 电容 CST820 / 电阻 / 无 |
| esp32_2432s028 | ST7789，240×320 | 无 | 电容 CST820 / 电阻 / 无 |
| esp32_2432s032 | ST7796，240×320 | 无 | 电容 GT911 / 电阻 / 无 |
| esp32_3248s035 | ST7796，320×480 | 无 | 电容 GT911 / 电阻 / 无 |
| jc2432w328 | ST7789，240×320 | 无 | 电容 CST820 / 电阻 / 无 |

这些 ESP32/C3 板型均没有 PSRAM。`GUITION_BOARD_NO_PSRAM` 自动启用；
误启用 `CONFIG_SPIRAM` 时组件 CMake 会终止构建并提示关闭。
示例使用内部 RAM 的 10 行 DMA 缓冲及任务栈，由 esp_lvgl_adapter 等待传输完成。
ST7796 240×320 参数沿用 ESP32-2432S032 的 Arduino 配置，需实机确认。

板型 YAML 的驱动依赖由 BMGR 写入生成组件清单，电容/电阻驱动通过
`$CONFIG{GUITION_TOUCH_*}` 条件按需解析；NONE 不下载对应触摸驱动。
通用 esp_lcd_touch 基础组件仍保留作为公共 API 依赖。

**初始化时按设备获取资源**：使用 `esp_board_manager_init_device_by_name()`
初始化 display_lcd、lcd_touch、lcd_brightness；示例 `display.c` 已提供此路径。
不要在这些多触摸变体板型调用 `esp_board_manager_init()` 批量初始化全部外设。
未选择的总线仅保留配置，不应初始化。S028 的 C/R 变体复用 GPIO32，
原生 BMGR 元数据报告此潜在冲突；互斥选择和按设备初始化避免同时占用。

XPT2046 使用原生驱动建议的 1 MHz（Arduino 宏写 40 MHz）；没有复位脚，
配置为 -1，避免驱动复位动作与 S028 的 SCK GPIO25 冲突。触摸坐标镜像沿用
每个 Arduino 变体；压力阈值 400，开启 ADC 到屏幕坐标转换，校准仍需实机。

## 已迁移的 ESP32-S3 板型

| board | 接口 / 屏幕 | 默认触摸 | 可选触摸 |
| --- | --- | --- | --- |
| esp32_4827s043 | RGB / ST7262，480×272 | XPT2046 | GT911 / XPT2046 / 无 |
| esp32_4848s040 | 三线 SPI+RGB / ST7701，480×480 | GT911 | GT911 / 无 |
| esp32_8048s043 | RGB / ST7262，800×480 | GT911 | GT911 / 无 |
| esp32_8048s050 | RGB / ST7262，800×480 | GT911 | GT911 / 无 |
| esp32_8048s070 | RGB / ST7262，800×480 | GT911 | GT911 / 无 |
| jc3248w535 | QSPI / AXS15231B，320×480 | AXS15231B | AXS15231B / 无 |
| jc3636k518 | QSPI / ST77916，360×360 | CST816S | CST816S / 无 |
| jc3636k718 | QSPI / ST77916，360×360 | CST816S | CST816S / 无 |
| jc3636w518 | QSPI / ST77916，360×360 | CST816S | CST816S / 无 |
| jc4827w543 | QSPI / ST77916，480×272 | GT911 | GT911 / XPT2046 / 无 |
| jc8048w550 | RGB / ST7262，800×480 | GT911 | GT911 / 无 |

S3 默认启用 OPI PSRAM 80 MHz、CPU 240 MHz、QIO Flash 80 MHz。
JC4827W543 为 4 MB Flash，其余板型为 16 MB，依据 Arduino 板卡设置截图。
JC3248W535 和三个 JC3636 原截图使用 Flash 120 MHz，本移植先采用 80 MHz。
不沿用 Arduino 文件系统分区，显示示例使用通用单应用分区。

RGB 采用 RGB565、16 位数据总线、两个 PSRAM 帧缓冲及 10 行 bounce buffer。
消费应用使用 adapter 的 `ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT` 与之匹配。
ST7701 控制器的 `bits_per_pixel=18` 与 RGB 数据流的 RGB565 分别配置，
保留 33 条原初始化命令。其三线 SPI IO 由 BMGR 持有和释放，驱动不能自动删除。
ESP32-8048S070 的 RGB 数据线及 DE/VSYNC 引脚与 S043/S050 不同，已分别配置。
ST7262 背光使用 GPIO 开关；其余 S3 背光使用 LEDC。

**AXS15231B 应用必须提交整帧**：原生 QSPI 驱动不发送局部地址窗口，
不能直接使用普通 SPI PARTIAL 刷新。仓库 `main/display.c` 使用 adapter
`ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_TE_HIGH_PERFORMANCE_CONFIG`：
原默认旋转 90°、TE GPIO38 下降沿、Tvdl/Tvdh 为 13/3 ms、QSPI 40 MHz/4 线，
采用 ASYNC_FULL staging。必须将 `te_sync.bounce_buffer_enabled=false`、
`bounce_buffer_rows=0`，让每次 panel draw 都写一个完整物理帧；IDF panel IO
内部按 SPI 最大传输长度分段并保持 CS，不在每行重新发送 RAMWR。
该板触摸为 I2C SCL=8/SDA=4，QSPI DC=-1。
AXS 的屏幕和触摸属于同一组件，选择 NONE 仍保留 LCD 驱动依赖。
TE 时序、旋转、颜色、触摸坐标和带宽需实机确认。

S3 固定屏幕驱动版本：ST77916 2.0.2、ST7701 2.0.2~2、AXS15231B 2.1.0。
未逐板构建固件；按接口选定三个代表板，其他板型通过原生代码生成和
真实 S3 工具链的工厂函数语法检查，不能视为各板运行验证。

## 现有 ESP32-P4 配置核查

P4 保留现有 IDF 移植，硬件参数优先于 Arduino 的不同配置。

| board | 实际 LCD / 触摸工厂 | 分辨率 | DPI 时钟 / DSI lane 速率 |
| --- | --- | --- | --- |
| jc1060p470 / jc1060p470_i_w_y | JD9165 / GT911 | 1024×600 | 52 MHz / 750 Mbps |
| jc4880p443 | ST7701 / GT911 | 480×800 | 34 MHz / 550 Mbps |
| jc8012p4a1 | JD9365 / GSL3680 | 800×1280 | V1 60 MHz / V2 70 MHz；1500 Mbps |

均为 DSI 2 lane、LDO3 2.5 V、三个帧缓冲，与示例 MIPI TRIPLE_PARTIAL 模式匹配。
JC1060 两版本共用显示工厂，音频引脚分别配置；LCD V2 暂预留。
IP101 引脚仅保留参考，BMGR 无原生支持，未添加以太网驱动或设备。
JC8012 在 menuconfig 的 `Guition JC8012P4A1 display` 中选择 LCD V1 / V2，
初始化表、DPI 时钟及垂直时序一起切换，默认 V2，无需修改源代码或 YAML。
屏幕选择与板型差异见 [JC8012](jc8012p4a1/readme.md)、
[JC1060](jc1060p470/readme.md)、[带外壳 JC1060](jc1060p470_i_w_y/readme.md)。
原阶段核查结果见下方记录；本次新增配置的编译检查不等同于实机验收。

需要区分以下现状：

- P4 使用原生 BMGR `lcd_touch`，尚未接入本组件新的 C/R/N 触摸菜单。
- JC4880 的 touch chip 元数据写 gls3680，实际工厂调用 GT911；触摸范围为
  800×1280，与屏幕 480×800 不同。本阶段保留已验证参数，需结合触摸坐标确认。
- JC8012 的 GSL3680 依赖 `guition-jc/esp_lcd_touch_gsl3680 ==0.1.0`，
  由组件中心直接下载，无需兄弟目录或驱动 `override_path`。
- JC8012 默认文件的 REV_MIN_100 在 IDF v6.1 中需要
  ESP32P4_SELECTS_REV_LESS_V3 配套。当前默认实际生成最低版本 3.1，
  不能据该默认文件假定固件支持 1.x；旧芯片按原板说明选择配套芯片选项。
- 现有 P4 默认依赖原工程 `partitions.csv`；隔离检查工具会复制该文件。
  独立例子 `examples/board_init` 自带适合最小 4 MB Flash 的分区表，
  应用可根据实际 Flash 和固件大小替换自己的分区表。

详细记录保存在迁移工程 `docs/guition-stage5-verification.md`。

## 触摸公共接口

`include/guition_touch.h` 提供：

- `guition_touch_selected_mode()`：读取编译期选择。
- `guition_touch_create(config, &device)`：获取 BMGR 总线引用、创建 IO、调用板型驱动工厂。
- `guition_touch_get_handle(device)`：取得标准 `esp_lcd_touch_handle_t`，用于 LVGL 输入适配。
- `guition_touch_destroy(device)`：先删除驱动，再删除 IO，最后归还 BMGR 总线引用。

板型以 BMGR `type: custom` 注册初始化/释放回调，将对应 C/R 变体转换成
`guition_touch_config_t`，按选中的模式提供 IO 配置和原生触摸驱动工厂函数。
初始化/释放与 BMGR 一致，从同一任务顺序调用。

BMGR custom 回调使用 `guition_touch_bmgr_create(config, void **handle)` 和
`guition_touch_bmgr_destroy(void *handle)` 桥接，而不是直接调用 create。
BMGR 会丢弃失败 custom 初始化的输出；桥接层会内部保留清理失败的对象，
在下一次初始化前重试清理。若初始化失败后不再重试，应在错误/关闭路径调用
`guition_touch_bmgr_recover()`。该桥接按一块板一个触摸设备设计。

无触摸或未迁移的配置可使用以下最小生命周期，不访问硬件：

```c
guition_touch_device_t *device = NULL;
if (guition_touch_selected_mode() == GUITION_TOUCH_MODE_NONE) {
    ESP_ERROR_CHECK(guition_touch_create(NULL, &device));
    assert(guition_touch_get_handle(device) == NULL);
    ESP_ERROR_CHECK(guition_touch_destroy(device));
}
```

无触摸仍返回一个非空生命周期对象，使 BMGR 的 custom 释放路径能正确查找设备；
其内部没有驱动、IO 或总线引用。已迁移板型包含触摸驱动依赖及工厂函数。

## SPI 复用与失败处理

共享 LCD/触摸的板型只声明一个 SPI peripheral，两个设备使用完全相同的
`bus_name`。适配器调用 `esp_board_periph_ref_handle()`，不会自行
`spi_bus_initialize()` 或 `spi_bus_free()`；专用触摸 SPI/I2C 总线也通过 BMGR 管理。
设备的 CS、频率和 mode 分别来自自己的 panel IO 配置。

共享型号：ESP32-2432S024、ESP32-2432S032、ESP32-3248S035、JC2432W328 的 R 变体。
ESP32-2432S028 的 R 变体使用另一个 host，不能合并；其他拓扑见项目配置核对文档。
无触摸不获取总线引用，因此不会影响仍在使用的 LCD。

驱动工厂返回错误时必须让输出句柄保持 NULL，或返回能够删除的部分初始化句柄。
适配器按驱动、IO、总线的顺序回滚。若释放本身也失败，create 会在返回错误的同时
返回一个非空 device；调用方必须保留它并重试 destroy，不能丢弃后直接再次初始化。
destroy 失败时也保留对象及未释放资源，允许重试。只有 destroy 成功后指针才失效。

`bus_name` 是借用的不可变字符串，必须活到 destroy 成功；板级静态配置满足该要求。
共享 host 的 GPIO 一致性和 CS 冲突检查由板级生成/验证工具负责，不以
“spi_bus_initialize 已初始化”错误代替验证。

## 打包与许可

在已激活的 ESP-IDF 环境中，从本组件根目录执行：

```bash
compote component pack --name guition_boards --dest-dir /tmp/guition-boards-dist
```

归档中不包含构建目录、下载组件、BMGR 生成文件及消费工程配置。
板库源码路径为 `components/boards/guition_boards`，组件中心命名空间为 `guition-jc`。
本地打包不会上传组件。公共适配采用 MIT；原板型文件保留其原始许可证，
详见 `NOTICE` 和 `LICENSES/`。外部驱动遵循各自发布包中的许可。

## SD 卡支持（2026-10-13）

`GUITION_BOARD_HAS_SD` 仅对 `asset/chip/chip-version.md` 明确记录 SD 引脚的型号启用，`GUITION_SD_ENABLED` 默认关闭。统一挂载点 `/sdcard`，20 MHz，不自动格式化。SDMMC 为 4 位，S3 slot 1、P4 slot 0；P4 保留 SD LDO4、MIPI LDO3（与引脚文档的 SD LDO3 记录存在冲突，待硬件确认）。未记录 SD 的旧板型配置已移除。

SPI/S3 生成脚本共同调用 `tools/guition/sd_boards.py` 的配置覆盖层；P4/遗留板型使用该脚本更新。重新生成后运行 `sd_boards.py --check` 和板库测试。

应用按设备名称初始化显示，不调用 BMGR 全量 init。`guition_sd_before_display()` 在 LCD/触摸之前运行；`guition_sd_mount/unmount()` 使用 BMGR `fs_sdcard` 配置与 SPI 引用，负责有限命令超时和可重试的资源释放。不要再调用 BMGR 对 `fs_sdcard` 的 init，以免重复挂载。生命周期调用由应用串行执行；共享触摸运行后重新挂载时，调用方应持有 LVGL adapter 锁暂停触摸轮询。

S028R 开启 SD 后使用原引脚的软件 SPI 触摸；两款 4827 共享 `spi_touch`，CS38/10 独立，容量4096字节。S040 先探测 SD SPI 模式，释放临时总线，再初始化 LCD，正式挂载 SD；探测失败时通过串口处理，不驱动 LCD 控制线。原显示/触摸行为在 SD 默认关闭时保持。完整应用示例见 `demo_ESP-IDF/general/sd_card`。
