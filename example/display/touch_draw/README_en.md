# Multi-Touch Drawing Example

[中文](README.md)

This example uses LVGL and `guition/guition_boards` to demonstrate touch drawing and simultaneous touch-coordinate display on Guition touchscreen development boards.

## Features

- **Touch drawing**: Press or drag on the white canvas to draw black dots at sampled positions. Samples are not connected with interpolated lines. Drawing remains after release.
- **Multiple coordinates**: Five touch pointers are enabled by default. The lower part of the screen shows a separate row for each active pointer, labeled `P1`–`P5`, with its `X` and `Y` coordinates. Pointers update independently; releasing one removes only its row. Numbers identify application-level virtual pointers, not permanent finger identities.
- **Clear button**: Tap **Clear** at the top of the screen to erase the canvas.

Coordinates are measured in pixels from the upper-left corner of the screen. The transparent coordinate overlay does not intercept touch input, so drawing remains possible underneath it. The clear-button area is excluded from drawing. Drawing is stored only in memory and is lost on restart.

## Development Environment

We recommend downloading and managing **ESP-IDF ≥ v6.1** with Espressif Installation Manager (EIM).

Activate the EIM-installed v6.1 environment in Bash or Zsh. Adjust the path and version to match your installation:

```shell
source ~/.espressif/tools/activate_idf_v6.1.sh
idf.py --version
```

Use a Guition board with a display and touchscreen, and select its matching chip target and board configuration. Component Manager downloads LVGL, ESP Board Manager, the display adapter, board components, and other dependencies automatically. Initial configuration requires internet access.

## Configure and Run

Run the following commands from the `touch_draw` project directory. This example uses the ESP32-P4 board `jc8012p4a1`:

```shell
idf.py set-target esp32p4
idf.py bmgr -b jc8012p4a1
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your serial port. Use `set-target` for initial configuration or when changing chips; routine source changes only require rebuilding. Run `idf.py bmgr -l` to list available boards. When switching boards, select the matching chip target and regenerate the board configuration.

ESP32-S31 is a preview target in ESP-IDF v6.1 and requires `--preview`. For example, with `jc8048f550`:

```shell
idf.py --preview set-target esp32s31
idf.py bmgr -b jc8048f550
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

### Touch Pointer Count

Open the following menu in `menuconfig`:

```text
Demo display → Touch pointer count
```

The setting is `CONFIG_DEMO_TOUCH_POINTERS`. It defaults to **5** and accepts **1–5**, also limited by the touch driver's `ESP_LCD_TOUCH_MAX_POINTS`. A value of 1 selects single-touch mode; larger values select multi-control mode.

The actual number of simultaneous contacts depends on the touch controller and driver. Selecting 5 does not add multi-touch capability to single-touch hardware. Drawing requires a board with touch support enabled.

## Usage

The example starts with a white canvas. Press and move one or more fingers to draw dots and view each active pointer's coordinates. Releasing a finger hides its coordinate row while preserving the drawing. Tap **Clear** to erase the canvas and continue drawing.
