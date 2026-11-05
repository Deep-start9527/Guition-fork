# 多点触摸画点示例

[English](README_en.md)

本示例基于 LVGL 和 `guition/guition_boards`，在 Guition 触摸显示开发板上演示触摸画点与多点坐标显示。

## 功能

- **触摸画点**：在白色画布上按下或滑动，在采样位置绘制黑色圆点，不在采样点之间插值连线。松开后保留绘制内容。
- **多点坐标**：默认支持 5 个触摸指针，屏幕下方逐行显示当前按下触点的 `P1`～`P5` 编号和 `X`、`Y` 坐标。各指针独立更新，松开一个触点只隐藏对应行。编号表示应用中的虚拟指针，不代表固定手指身份。
- **一键清屏**：点击顶部的 **Clear** 按钮清空画布。

坐标以屏幕左上角为原点，单位为像素。坐标文字透明叠加，不拦截触摸，其覆盖区域仍可画点；清屏按钮区域不参与绘图。绘制内容仅保存在内存中，重启后清空。

## 开发环境

推荐使用 Espressif Installation Manager（EIM）下载和管理 **ESP-IDF ≥ v6.1**。

在 Bash 或 Zsh 中启用 EIM 安装的 v6.1 环境,路径与版本根据自身具体路径进行修改：

```shell
source ~/.espressif/tools/activate_idf_v6.1.sh
idf.py --version
```

使用带显示屏和触摸屏的 Guition 开发板，并选择匹配的芯片目标与板型。组件管理器自动下载 LVGL、ESP Board Manager、显示适配器和板级组件等依赖，首次配置需要联网。

## 配置与运行

在 `touch_draw` 工程目录执行。以下以 ESP32-P4 开发板 `jc8012p4a1` 为例：

```shell
idf.py set-target esp32p4
idf.py bmgr -b jc8012p4a1
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

将 `PORT` 替换为实际串口。`set-target` 用于首次配置或切换芯片；日常修改后可直接构建。使用 `idf.py bmgr -l` 查看可用板型，切换开发板时选择对应的芯片目标并重新生成板级配置。

ESP32-S31 在 ESP-IDF v6.1 中属于预览目标，需要添加 `--preview`。例如 `jc8048f550`：

```shell
idf.py --preview set-target esp32s31
idf.py bmgr -b jc8048f550
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

### 触摸点数

在 `menuconfig` 中打开：

```text
Demo display → Touch pointer count
```

对应配置项为 `CONFIG_DEMO_TOUCH_POINTERS`，默认 **5**，可设为 **1～5**，上限同时受触摸驱动的 `ESP_LCD_TOUCH_MAX_POINTS` 限制。设置为 1 时使用单点模式，大于 1 时使用多指针控制模式。

实际可同时识别的触点数量取决于触摸控制器和驱动。将配置设为 5 不会使单点触摸硬件具备多点能力；禁用触摸或使用不带触摸的板型时无法画点。

## 使用方法

启动后显示白色画布。用一个或多个手指按下、移动，即可画点并查看各触点坐标。松开手指后，对应坐标行消失，已绘制的黑点保留。点击 **Clear** 清空画布后可继续绘制。
