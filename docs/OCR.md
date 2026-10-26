# 本地 OCR

## 模型与语言

RapidOCR 3.10.0／ONNX Runtime 1.31.0，CPU 推理：

| 设置语言 | 检测／识别 |
| --- | --- |
| 中文／英文／日文混合 | PP-OCRv6 small |
| 法语 | 同一 PP-OCRv6 多语种模型 |
| 韩语 | 共用检测与方向分类，PP-OCRv5 Korean 识别 |
| 俄语 | 共用检测与方向分类，PP-OCRv5 East Slavic 识别 |

设置→OCR 切换 RapidOCR／视觉 AI。模型在安装阶段准备，实际识别离线。当前语言的引擎复用，切换语言替换旧引擎，空闲五分钟释放。

## 安装与文件

```bash
./scripts/install-ocr.sh
```

依赖 uv，脚本准备 Python 3.12；默认安装到 `~/.local/share/mokou-screenshot/ocr`。

| 文件 | 责任 |
| --- | --- |
| requirements.txt／requirements.lock | 顶层依赖及完整固定版本／哈希 |
| worker.py | 本地协议适配与模型运行 |
| extra-models.json | 韩俄模型的固定 URL 与 SHA256 |
| setup_models.py | 安装时下载与校验 |
| scripts/install-ocr.sh | 独立环境、模型与 worker 安装 |
| runtime/venv、runtime/models | 安装后的环境与额外模型；不属于源码 |

三个随包模型及两个额外模型都校验 SHA256。基础环境约 348 MiB，额外模型另占磁盘。运行版使用自己的 venv，与系统 Python 分开；源码仓库不包含环境与权重。

## 管道协议

OcrClient 按需启动 worker，使用一行一个 JSON 的 stdin／stdout，不创建服务器或临时截图文件。

请求：

```json
{"png":"base64 PNG","language":"multi"}
```

language 为 multi、french、korean 或 russian。成功响应：

```json
{"coordinate_system":"normalized","box_format":"xywh","blocks":[{"text":"original","bbox":[0.1,0.2,0.5,0.1]}],"elapsed_ms":50}
```

失败响应是稳定错误码。父进程显示中文状态；日志不含截图和识别正文。取消活动请求会终止 worker，重试重新加载；请求完成后释放图像数据，模型留待复用。

## 修改与复核

增加语言要同步 Settings 的允许值、设置列表、worker 的路由、模型清单及 ocr-check 实际样本。更改推理参数先用合成文本复核，再按需使用真实样本。

```bash
MOKOU_REAL_OCR=1 \
MOKOU_OCR_DIR="$HOME/.local/share/mokou-screenshot/ocr" \
  ./build/ocr-check -platform offscreen
```

清晰横排语言样本与耗时见 [验证记录](VERIFICATION.md)。倾斜／竖排、艺术字及复杂阅读顺序仍需专门样本；显示层使用轴对齐文字框。最长边超过 1920 时会缩小识别，小字宜缩小选区。

参考：[RapidOCR 模型](https://rapidai.github.io/RapidOCRDocs/main/en/model_list/)、[安装](https://rapidai.github.io/RapidOCRDocs/main/en/install_usage/rapidocr/install/)。
