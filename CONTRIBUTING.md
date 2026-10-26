# 参与开发

先读 [开发入口](AGENTS.md)、[架构](docs/ARCHITECTURE.md) 与 [构建说明](docs/DEVELOPMENT.md)。

- C++20／Qt 6 Widgets；按 .clang-format 格式化，仅修改相关代码。
- 捕获、文档、识别调度、传输、界面和录屏各自承担明确职责。优先使用已有模块和 Qt／KDE 的能力。
- 识别结果通过 Document 接口修改；新业务放在 RecognitionController。
- 控制流程变化运行 ctest；桌面交互变化补 Wayland 验证。新增检查应针对实际故障或行为边界。
- 同步对应主题文档与 CHANGELOG，说明变更原因、结果和验证。

报告问题时提供版本、Fedora／Plasma 版本、显示器缩放、复现步骤和实际结果。涉及识别时，可用不含个人信息的样本；不要上传 API Key、完整配置或私人截图。

PR 描述写清问题、行为变化和相关检查。贡献按项目 MIT 许可证提供；第三方文件保留原有许可。
