# 验证

版本 **0.4.3**，2026-10-26。环境：Fedora 44／KDE Plasma Wayland，Qt 6.11.2、KPipeWire 6.7.5、GCC 16。

## 自动检查

`ctest --test-dir build --output-on-failure` 六组通过，约 3.3 秒；真实 Wayland 编辑检查通过。

| 检查 | 覆盖 |
| --- | --- |
| core-check | 坐标、合成、撤销、输出、API、结果失效、视频时长 |
| editor-check | 绘制、跨框选择与边界、展开命中、菜单、识图卡片 |
| lifecycle-check | 设置、会话、延迟录屏、OCR → 文本翻译 |
| audio-check | 音源查询、停止、失败、复用与大量进程输出 |
| ocr-check | worker 复用／取消，可选真实语言模型 |
| repository-check | 隔离安装／卸载、链接及脚本语法 |

检查使用临时配置与模拟服务。媒体样本按可用编码器执行；GitHub CI 结果在推送后产生。

## 桌面与模型

实机路径已覆盖截图／复制、MP4／WebM、系统声音、停止封装、涂抹式马赛克、文字输入、跨框复制与工具菜单。原生编辑检查使用合成画布并恢复剪切板。

本地中英日／法韩俄以清晰横排样本验证；真实模型检查命令见 [OCR](OCR.md)。多屏混合缩放、输入法候选窗、长时音画同步与复杂竖排仍待覆盖。复现入口见 [开发](DEVELOPMENT.md)。
