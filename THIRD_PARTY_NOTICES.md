# 第三方软件

MokouScreenshot 的自有源码、图标和文档采用 [MIT](LICENSE)。以下依赖保留各自许可；MIT 不替代它们的许可证。

| 组件 | 使用方式 | 上游许可与来源 |
| --- | --- | --- |
| Qt 6 | 动态链接 Widgets、DBus、Network、WaylandClient | [LGPL-3.0／GPL／商业许可](https://www.qt.io/licensing/) |
| KDE KGlobalAccel | 动态链接，全局快捷键 | [LGPL-2.0-or-later](https://invent.kde.org/frameworks/kglobalaccel) |
| KPipeWire | 动态链接，视频编码 | [LGPL-2.1／LGPL-3.0 等](https://invent.kde.org/plasma/kpipewire) |
| LayerShellQt | 动态链接，Wayland 覆盖层 | [LGPL-3.0-or-later 等](https://invent.kde.org/plasma/layer-shell-qt) |
| plasma-wayland-protocols | 构建时生成区域录屏协议绑定 | [LGPL-2.1-or-later](https://invent.kde.org/libraries/plasma-wayland-protocols/-/blob/master/src/protocols/zkde-screencast-unstable-v1.xml) |
| FFmpeg | 系统独立进程，录音与封装 | [LGPL／GPL，依发行版构建选项](https://ffmpeg.org/legal.html) |
| RapidOCR 3.10.0 | 独立 Python worker，安装时获取 | [Apache-2.0](https://github.com/RapidAI/RapidOCR/blob/v3.10.0/LICENSE) |
| ONNX Runtime | Python OCR 推理运行时 | [MIT](https://github.com/microsoft/onnxruntime/blob/main/LICENSE) |
| PP-OCR 模型 | 安装时获取，不放入源码包 | [PaddleOCR](https://github.com/PaddlePaddle/PaddleOCR)、[RapidOCR 模型源](https://www.modelscope.cn/models/RapidAI/RapidOCR) |

区域录屏协议的版权声明为 Copyright 2020–2021 Aleix Pol Gonzalez。生成绑定保留上游标识；对应 [LGPL-2.1-or-later 全文](LICENSES/LGPL-2.1-or-later.txt) 随项目和安装文件提供。

源码包不包含系统库、Python 依赖或模型权重。Python 包的许可证保留在安装环境的 dist-info 中，模型 URL 与 SHA256 记录于 [模型清单](ocr/extra-models.json)。重新分发运行环境或模型时，应同时携带对应上游许可与声明。
