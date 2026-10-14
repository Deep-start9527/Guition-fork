使用 ESP-IDF 6.1。板级支持由组件中心的 `guition-jc/guition_boards: "*"` 提供，首次解析依赖时下载满足工程约束的最新版本。

以 `jc8012p4a1` 为例，在工程目录执行：

```shell
. "$IDF_PATH/export.sh"
export IDF_EXTENSION_ALLOW_UNTRUSTED=1
idf.py set-target esp32p4
idf.py bmgr -b jc8012p4a1
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

`IDF_EXTENSION_ALLOW_UNTRUSTED=1` 用于加载 ESP Board Manager 的 `bmgr` 命令扩展。将 `PORT` 替换为实际串口。`idf.py bmgr -l` 可列出支持的开发板；使用其他板型时，选择对应的芯片目标和开发板名称。

依赖解析后由 `dependencies.lock` 锁定版本。需要更新组件时执行 `idf.py update-dependencies`，再执行 `idf.py bmgr -b <board>` 和 `idf.py build`。
