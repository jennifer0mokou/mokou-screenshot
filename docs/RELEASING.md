# 发布

1. 更新 CMakeLists.txt 的 project VERSION，同步 README、CHANGELOG 和 VERIFICATION。
2. 按 [开发说明](DEVELOPMENT.md) 构建并运行 ctest；桌面与模型检查按改动范围选择。
3. 检查暂存内容后提交，用 Git 的已提交树生成源码包。

```bash
version=0.4.3
mkdir -p dist
git archive --format=tar.gz --prefix="mokou-screenshot-$version/" \
  -o "dist/mokou-screenshot-$version.tar.gz" HEAD
(cd dist && sha256sum "mokou-screenshot-$version.tar.gz" > SHA256SUMS)
DESTDIR="$PWD/.artifacts/stage" cmake --install build
```

目标仓库为 jennifer0mokou/mokou-screenshot。推送 main 后查看 Fedora CI 结果，再创建 v版本号 标签及 Release，附源码包、SHA256SUMS 和对应 CHANGELOG。

源码包包含代码、检查、文档、许可和合成界面图；环境与模型由安装流程获取。当前发布源码，二进制发行另行验证目标 Fedora／架构、运行依赖和桌面权限。
