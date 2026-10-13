# ESP LCD ST7102

基于 `esp_lcd` 和 `esp_lcd_touch` 的 ST7102 显示与触摸驱动。

| LCD 控制器 | 通信接口 | 组件名称 |
| :---: | :---: | :---: |
| ST7102 | 3-wire SPI + RGB（显示）、I2C（触摸） | esp_lcd_st7102 |

要求 ESP-IDF 5.3 及以上版本，显示功能需要芯片支持 RGB LCD 外设。默认初始化参数适用于 480×480 面板。

## Add to project

将解压后的 `esp_lcd_st7102` 目录放入工程的 `components/` 下，组件管理器会解析触摸依赖。
下面的 3-wire SPI 示例还需添加：

```bash
idf.py add-dependency "espressif/esp_lcd_panel_io_additions"
```

## Example use

### RGB Interface

3-wire SPI 发送控制命令，RGB 总线传输像素。先按板卡接线配置 `rgb_config` 的 RGB GPIO、时钟和帧缓冲；下列 `LCD_*` 宏由应用定义。

```c
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7102.h"

esp_lcd_panel_io_3wire_spi_config_t io_config = {
    .line_config = {
        .cs_io_type = IO_TYPE_GPIO, .cs_gpio_num = LCD_SPI_CS,
        .scl_io_type = IO_TYPE_GPIO, .scl_gpio_num = LCD_SPI_SCK,
        .sda_io_type = IO_TYPE_GPIO, .sda_gpio_num = LCD_SPI_SDA,
    },
    .expect_clk_speed = 160000,
    .spi_mode = 0,
    .lcd_cmd_bytes = 1,
    .lcd_param_bytes = 1,
    .flags.use_dc_bit = 1,
};
esp_lcd_panel_io_handle_t display_io = NULL;
ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config, &display_io));

// rgb_config 是已按板卡配置好的 esp_lcd_rgb_panel_config_t，使用 16 位 RGB 总线。
rgb_config.timings = (esp_lcd_rgb_timing_t)ST7102_480_480_PANEL_RGB_TIMING();
st7102_vendor_config_t vendor_config = {
    .rgb_config = &rgb_config,
    // init_cmds 为 NULL 时使用内置初始化表。
};
esp_lcd_panel_dev_config_t panel_config = {
    .reset_gpio_num = LCD_RST,  // 不使用复位引脚时设为 -1
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = 16,
    .vendor_config = &vendor_config,
};
esp_lcd_panel_handle_t panel = NULL;
ESP_ERROR_CHECK(esp_lcd_new_panel_st7102(display_io, &panel_config, &panel));
ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
```

显示 IO 和自定义初始化表须保留至面板删除；触摸控制器须已有可运行固件。

### I2C Touch

使用已创建的 I2C 主机总线 `touch_bus` 初始化触摸。

```c
#include "driver/i2c_master.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_touch_st7102.h"

esp_lcd_panel_io_i2c_config_t touch_io_config = ESP_LCD_TOUCH_IO_I2C_ST7102_CONFIG();
esp_lcd_panel_io_handle_t touch_io = NULL;
ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(touch_bus, &touch_io_config, &touch_io));

esp_lcd_touch_config_t touch_config = {
    .x_max = 480,
    .y_max = 480,
    .rst_gpio_num = GPIO_NUM_NC,
    .int_gpio_num = GPIO_NUM_NC,
};
esp_lcd_touch_handle_t touch = NULL;
ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_st7102(touch_io, &touch_config, &touch));
```

