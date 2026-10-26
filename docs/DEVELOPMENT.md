# 开发

目标环境为 Fedora KDE Plasma Wayland。需要 C++20、CMake ≥ 3.24、Qt ≥ 6.8、KPipeWire ≥ 6.7。模拟检查使用系统 Python ≥ 3.9，本地 OCR 使用独立 Python 3.12。

## 构建与安装

```bash
sudo dnf install gcc-c++ cmake ninja-build extra-cmake-modules \
  qt6-qtbase-devel qt6-qtwayland-devel kf6-kglobalaccel-devel \
  kpipewire-devel layer-shell-qt-devel plasma-wayland-protocols-devel

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

安装使用 `./scripts/install.sh`，只构建应用目标；`--without-ocr` 跳过 OCR 环境与模型。默认前缀 `~/.local`，可用 MOKOU_INSTALL_PREFIX 指定绝对路径。更新前退出运行实例，脚本同时更新应用菜单、KDE 权限声明和登录待命入口。

录屏需要 FFmpeg 的 setts 过滤器，声音需要 pulse 输入、Opus／AAC 与 pactl。Fedora 可安装 ffmpeg-free、pulseaudio-utils；用 `ffmpeg -encoders`／`ffmpeg -devices` 查看能力。MP4 需要 KPipeWire 可用的 H.264 编码器。OCR 依赖 uv，见 [OCR](OCR.md)。

## 运行与调试

| 参数 | 操作 |
| --- | --- |
| --capture | 区域截图，默认操作 |
| --settings | 设置 |
| --background | 托盘待命 |
| --demo | 合成画布 |
| --stop | 停止录屏 |
| --quit | 正常退出，先完成活动录屏 |

第二次启动通过用户 D-Bus 转交现有实例。桌面 ID 为 io.github.fedokou.MokouScreenshot；真实捕获与录屏使用安装后的 mokou-screenshot。

## 检查

| 程序 | 覆盖 |
| --- | --- |
| core-check | 坐标、图像合成、拼图、输出、API、结果失效与视频时长 |
| editor-check | 框选、标注、跨框选择、展开文字、菜单与识图卡片 |
| lifecycle-check | 私有 D-Bus、设置、会话与识别流程、延迟录屏 |
| audio-check | 模拟音源与 FFmpeg 的异步生命周期 |
| ocr-check | 模拟 worker；可选真实模型 |
| repository-check | 隔离安装／卸载、文档链接与脚本语法 |

默认检查使用临时配置、私有 D-Bus 和模拟进程。控制流程改动运行 ctest；桌面交互改动补真实 Wayland 检查：

```bash
mkdir -p .artifacts
MOKOU_NATIVE_CHECK=1 MOKOU_PREVIEW="$PWD/.artifacts/editor.png" \
  ./build/editor-check -platform wayland
```

截图／录屏诊断会捕获当前桌面，结果保存在指定路径：

```bash
~/.local/bin/mokou-screenshot --capture-check "$PWD/.artifacts/desktop.png"
~/.local/bin/mokou-screenshot --record-check "$PWD/.artifacts"
```

录屏诊断约两秒，可加 --check-audio、--check-webm 或 --check-cancel-start。真实模型检查见 [OCR](OCR.md)，当前结果见 [验证记录](VERIFICATION.md)。

## 配置与维护

配置为 `~/.config/fedokou/MokouScreenshot.conf`，支持 XDG_CONFIG_HOME。工具栏保存文字样式、阅读背景和取色倍率，画笔样式用于当前编辑。Key 只在内存或环境变量中。

C++ 使用 .clang-format。生成协议代码留在 build；版本只改 CMakeLists.txt 并同步发布文档。模块约定见 [架构](ARCHITECTURE.md)，贡献入口见 [CONTRIBUTING](../CONTRIBUTING.md)。

卸载运行 `./scripts/uninstall.sh`，保留用户数据与 OCR 环境。录屏失败的数据留在录屏目录的 .mokou-随机字符文件夹。
