# Changelog

## 0.1.3

- Add the `display_lv_demo_widgets` example with LVGL Widgets, Board Manager display initialization and touch input.
- Register the example at `example/display/display_lv_demo_widgets` for publication to the ESP Component Registry.
- Correct the upstream repository metadata to `Guition-JC/Guition` and the component path to `components/board/guition_boards`.
- Exclude local reference documents, development tools and editor settings from release archives.
- Update the README version, installation command and display example link.

## 0.1.2

- Add the `jc8048f543` ESP32-S31 board definition with the same peripheral, device and SDK configuration as `jc8048f550` for a different physical screen size.
- Include all three ESP32-S31 boards in the board catalog.
- Standardize board YAML indentation, list spacing, inline scalar arrays, hexadecimal I2C addresses and double-quoted strings.
- Preserve configuration values, data types, field order and YAML anchors and aliases during formatting.
- Document YAML formatting conventions and the 24 board configurations, including ESP32-S31.
- Synchronize the component version, README installation command and example dependency at `0.1.2`.

## 0.1.1

- Add target esp32s31

## 0.1.0 package refresh (2026-10-13)

- Rename the component directory to `guition_boards` under `components/boards`.
- Use published `guition-jc/esp_lcd_touch_gsl3680 ==0.1.0` for JC8012.
- Document published `guition-jc/esp_lcd_axs15260 ==1.0.0` for applications using that controller.
- Update registry namespace, repository metadata, package exclusions and local example.
- Preserve all board hardware settings and public C APIs.


## 0.1.0 — migration preview

- Add 17 ESP32/C3/S3 board definitions, configurable touch and shared SPI.
- Preserve the existing hardware-verified ESP32-P4 board parameters.
- Use the external `guition-jc/esp_lcd_touch_gsl3680` 0.1.0 dependency for JC8012.
- Map both JC1060 Arduino SKUs to the existing `jc1060p470` BMGR definition.
- Add a standalone initialization example and registry archive verification.
- Hardware validation of new ESP32/C3/S3 configurations remains pending.
