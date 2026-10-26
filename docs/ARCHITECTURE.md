# 架构

## 结构

C++20／Qt 6 Widgets 主进程负责桌面交互，按需启动 Python OCR worker。系统 Qt／KDE／FFmpeg 提供桌面与媒体能力。生产代码编入内部静态库 mokou-core，应用与检查链接同一实现。

| 模块 | 责任 |
| --- | --- |
| main | 程序标识、CLI、单实例 D-Bus、诊断入口；版本来自 CMake |
| application | 托盘、编辑会话、模态窗口、保存、录屏启动协调 |
| capture | KWin ScreenShot2 异步捕获与图像管道 |
| document | 冻结图像、逻辑坐标、标注、撤销、合成缓存、识别结果 |
| recognition | OCR／翻译／识图的前置配置、调度、取消、结果提交与设置失效 |
| ai | OpenAI 兼容 HTTP 传输、取消、响应解析 |
| ocr／worker.py | 本地模型进程、请求管道、复用和空闲释放 |
| textresult | 坐标与翻译 ID 校验、格式转换 |
| overlay | 每个显示器的框选／绘制交互、阅读层与工具栏布局 |
| editortoolbar | 分组工具栏、子选项、可用状态；通过信号请求操作 |
| annotationinput／textoverlay／readingpanel | 输入标注、统一跨框阅读与就地展开、自动识图卡片 |
| panels | 设置与拼图窗口 |
| imageoutput | PNG 原子保存与同份图像剪切板 |
| recorder／audiocapture | KWin 区域视频、音源查询／录音、最终封装与发布；不用的子进程输出直达空设备 |
| shortcuts／unixsignals | PrtScn 正常注册；系统退出信号转交事件循环 |
| settings | 普通偏好与文件名；Key 为内存字段 |

## 数据流

```text
PrtScn → Capture → Document → Overlay
                           → 裁切合成 → PNG + 剪切板
                           → RecognitionController
                              ├ OCR → 本地 worker／视觉 API → Document 结果
                              ├ 翻译 → 复用 OCR 文字 → 文本 API → Document 译文
                              └ 识图 → 选区图片 API → Document 描述
```

Application 只协调窗口与会话；RecognitionController 协调识别业务；传输客户端不操作窗口。Overlay 从 Document 绘制，通过信号请求外部操作。

## 图像与坐标

- 编辑坐标采用虚拟桌面逻辑坐标，desktopOrigin 保存全局原点；每个 Overlay 使用显示器偏移。
- 原图保留物理分辨率，裁切时按 image.size／logicalSize 转换。录屏交给 KWin 的仍是全局逻辑坐标。
- 标注以向量保存，渲染缓存按 contentRevision 更新。每笔记录创建时的选区裁切，预览与导出共用 paintMark。
- 马赛克用 QPen 的纹理画笔，沿 QPainterPath 涂抹；采样此前已合成画面，保留先前遮挡。纹理网格固定为约 12 逻辑像素，预览纹理在按下时生成一次。
- 文字标注保存 QFont、颜色和基线；OCR／译文及面板是阅读层。
- 拼图按物理像素布局，预览只分配预览画布，导出才分配完整图像。

## 状态与生命周期

| 所有者 | 状态与规则 |
| --- | --- |
| Document | 内容／选区改变增加 revision 并清除识别结果；工具与样式不增加 revision |
| Document | 识别数据私有，通过 setTextBlocks／setDescription／clearTranslations 等更新 |
| Document | resultsChanged 同步阅读层和识图卡片；changed 刷新一般界面，每次编辑一次 |
| RecognitionController | 记录请求 revision；内容变化、模态操作或关闭会话时取消 |
| Application | editorGeneration 隔离旧窗口；recordingRequest 隔离延迟启动 |
| Application | 进入设置／拼图先取消待启动录屏，再进入模态事件循环 |
| Capture | 独立请求代号、超时、文件描述符；迟到回调不进入新请求 |
| Recorder | Idle → Starting → Recording → Finishing → Idle；退出先完成文件 |

录屏编码对象停止前保留临时目录。最终封装写完整临时文件后重命名发布，失败数据保留。静止画面用 setts 延长末包，保留 PTS／DTS，视频不重新编码。

## 修改入口

- 新编辑工具：Document 的 Mark／渲染 + Overlay 交互。
- 新识别业务：RecognitionController；新响应形式先转换为 TextBlock。
- OCR 模型：worker、模型清单、设置语言项及真实语言检查。
- 文字／取色偏好：EditorToolbar → Application 保存 Settings；其余设置：SettingsDialog；影响识别缓存的字段在 recognition.settingsChanged 处理。
- 新桌面后端：Capture／Recorder；沿用文档与编辑层。

交互的布局、选区和键盘规则见 [交互设计](INTERACTION.md)。

当前采用 KDE ScreenShot2、LayerShellQt、KPipeWire 与 screencast 协议 v6。接口参考：[KWin](https://invent.kde.org/plasma/kwin/-/blob/Plasma/6.7/src/plugins/screenshot/screenshotdbusinterface2.cpp)、[screencast](https://invent.kde.org/libraries/plasma-wayland-protocols/-/blob/master/src/protocols/zkde-screencast-unstable-v1.xml)、[KPipeWire](https://invent.kde.org/plasma/kpipewire/-/blob/Plasma/6.7/src/pipewirebaseencodedstream.cpp)。
