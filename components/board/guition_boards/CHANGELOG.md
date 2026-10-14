# Changelog

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
