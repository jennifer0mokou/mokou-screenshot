# 开发

所有命令从工程根目录运行。要求 Qt ≥ 6.8、KPipeWire ≥ 6.7、CMake ≥ 3.24 和 C++20 编译器；目标环境为 Fedora KDE Plasma Wayland。OCR 使用独立 Python 3.12 环境，模拟检查只需系统 Python ≥ 3.9。

## 依赖与构建

```bash
sudo dnf install gcc-c++ cmake ninja-build extra-cmake-modules \
  qt6-qtbase-devel qt6-qtwayland-devel kf6-kglobalaccel-devel \
  kpipewire-devel layer-shell-qt-devel plasma-wayland-protocols-devel

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

录屏需 FFmpeg setts 过滤器；声音需 pulse 输入、Opus／AAC 和 pactl。Fedora 可先安装 ffmpeg-free、pulseaudio-utils，并用 `ffmpeg -encoders`／`ffmpeg -devices` 检查本机能力；MP4 还需要系统 KPipeWire 可用的 H.264 编码器。OCR 安装需 uv，脚本会准备 Python 3.12。`BUILD_TESTING=OFF` 可只构建应用。

## 安装与运行

```bash
./scripts/install.sh
~/.local/bin/mokou-screenshot --background
```

脚本构建、安装 OCR、注册菜单权限及登录待命入口。`--without-ocr` 跳过 OCR 环境和模型，可稍后独立安装；该模式不需要 uv。真实桌面捕获／录屏使用安装后的二进制；KWin 根据其路径与 desktop 元数据检查权限。默认前缀 `~/.local`，可通过 MOKOU_INSTALL_PREFIX 设置绝对路径。桌面 ID 为 `io.github.fedokou.MokouScreenshot`，程序名为 `mokou-screenshot`。

更新前先退出运行实例。正在编辑或录屏时保留当前会话，完成后再更新。

| CLI | 操作 |
| --- | --- |
| --capture | 区域截图 |
| --settings | 设置 |
| --background | 托盘待命 |
| --demo | 合成画布演示 |
| --stop | 停止录屏 |
| --quit | 正常退出并完成活动录屏 |

第二次启动通过用户 D-Bus 转交现有实例。

## 检查选择

| 程序 | 覆盖 |
| --- | --- |
| core-check | 坐标、合成／马赛克、拼图、保存／剪切板、API 协议、文档通知、静止视频 |
| editor-check | 框选、标注、跨框正反选字、复制、长文字展开、内嵌菜单与识图卡片 |
| lifecycle-check | 私有 D-Bus 与配置；会话竞态、模态／延迟录屏、设置、识别完整流程 |
| audio-check | 模拟 pactl／FFmpeg；查询、停止、失败与复用 |
| ocr-check | 模拟 worker；复用、取消；可选真实语言模型 |
| repository-check | 安装／卸载隔离检查、文档链接、Python 与 shell 语法 |

默认 offscreen 检查不操作桌面。控制流程变更运行 ctest；低影响修改运行相关检查即可。

```bash
# 真实模型：清晰合成文本，记录耗时与结果数量
MOKOU_REAL_OCR=1 \
MOKOU_OCR_DIR="$HOME/.local/share/mokou-screenshot/ocr" \
  ./build/ocr-check -platform offscreen

# 真实 Wayland：合成覆盖层，结束恢复剪切板
mkdir -p .artifacts
MOKOU_NATIVE_CHECK=1 \
MOKOU_PREVIEW="$PWD/.artifacts/native-editor.png" \
  ./build/editor-check -platform wayland
```

桌面诊断保存在 .artifacts，以下命令会捕获当前桌面或主屏中心区域：

```bash
mkdir -p .artifacts
~/.local/bin/mokou-screenshot --capture-check "$PWD/.artifacts/desktop.png"
~/.local/bin/mokou-screenshot --record-check "$PWD/.artifacts"
```

录屏诊断约两秒，可追加 --check-audio、--check-webm、--check-cancel-start。结果用 ffprobe 检查。检查通过后继续交付，不对未改动路径反复全量测试。

## 维护约定

C++ 使用 .clang-format。生成协议代码只在 build 中。版本只改 CMakeLists.txt 的 project VERSION。新增结果写入使用 Document 接口，新增识别状态放 RecognitionController。

检查使用临时配置、私有 D-Bus 和模拟进程。GitHub CI 在 Fedora 44 容器构建与执行六组检查，不安装模型；只构建应用时可设置 BUILD_TESTING=OFF。

## 配置、卸载与故障数据

配置默认 `~/.config/fedokou/MokouScreenshot.conf`，支持 XDG_CONFIG_HOME。Key 不序列化。文字字体／字号／颜色、阅读背景与取色倍率从工具栏调整并保存。画笔样式用于当前编辑。

退出后运行 `./scripts/uninstall.sh`。它移除主程序、菜单、图标和自动启动项，保留用户数据及 OCR 环境。PrtScn 在 KDE 快捷键设置中管理。

录屏失败的数据保存在录屏目录的 `.mokou-随机字符/`。本地 OCR 可用 `./scripts/install-ocr.sh` 单独修复；模型和 worker 详见 [OCR](OCR.md)。
