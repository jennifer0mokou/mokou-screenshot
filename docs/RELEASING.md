# 发布

## 版本与构建

版本只改 CMakeLists.txt 的 project VERSION，同步 README、CHANGELOG 和 VERIFICATION。首次公开发布使用 v0.4.2。

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
DESTDIR="$PWD/.artifacts/stage" cmake --install build
```

CI 在 Fedora 44 容器运行相同构建与检查，不获取用户配置或模型。真实 Wayland／录屏和模型验证按 [开发说明](DEVELOPMENT.md) 在桌面上进行。

## 源码包

使用 Git 已提交的树打包，自然排除构建、模型、虚拟环境、截图、配置和本地开发记录：

```bash
mkdir -p dist
git archive --format=tar.gz --prefix=mushroom-shot-0.4.2/ \
  -o dist/mushroom-shot-0.4.2.tar.gz HEAD
sha256sum dist/mushroom-shot-0.4.2.tar.gz > dist/SHA256SUMS
```

仓库包含安装脚本、检查、文档、自有图标与合成界面示例。模型通过带 SHA256 的安装流程取得，不随 Git 或源码包分发。

## GitHub

确定仓库名称与公开范围，设置 origin 后推送 main。GitHub Actions 完成后，再创建 v0.4.2 标签与 Release，附源码包和 SHA256SUMS；发布说明取自 CHANGELOG 的对应版本。

当前只准备源码发行。二进制包需要另行确定 Fedora／架构目标，包含相应第三方许可并验证安装及桌面权限，不把当前机器的构建目录直接上传。
