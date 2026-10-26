# 本地 OCR

RapidOCR 3.10.0／ONNX Runtime 1.31.0，使用 CPU 推理。

| 语言 | 模型 |
| --- | --- |
| 中英日混合、法语 | PP-OCRv6 small |
| 韩语 | 共用检测／方向分类，PP-OCRv5 Korean 识别 |
| 俄语 | 共用检测／方向分类，PP-OCRv5 East Slavic 识别 |

设置→OCR 选择 RapidOCR／视觉 AI。引擎按语言复用，切换语言替换旧引擎，空闲五分钟释放。

## 安装与维护

运行 `./scripts/install-ocr.sh`；依赖 uv，准备独立 Python 3.12，默认位置为 ~/.local/share/mokou-screenshot/ocr。

- requirements.txt 声明顶层依赖，requirements.lock 固定全部版本与哈希。
- 三个随包模型在安装脚本校验；韩俄模型由 extra-models.json 固定 URL／SHA256，setup_models.py 下载并校验。
- worker.py 只做离线推理；模型与 venv 不放入源码包。

增加语言要同步 Settings、设置列表、worker 路由、模型清单及 ocr-check 样本。

## 管道协议

OcrClient 按需启动 worker，使用单行 JSON 的 stdin／stdout。

```json
{"png":"base64 PNG","language":"multi"}
```

language 可为 multi、french、korean、russian。成功响应：

```json
{"coordinate_system":"normalized","box_format":"xywh","blocks":[{"text":"original","bbox":[0.1,0.2,0.5,0.1]}],"elapsed_ms":50}
```

失败返回稳定错误码。取消会终止 worker；完成后释放请求数据，模型保持待用。真实模型检查：

```bash
MOKOU_REAL_OCR=1 MOKOU_OCR_DIR="$HOME/.local/share/mokou-screenshot/ocr" \
  ./build/ocr-check -platform offscreen
```

显示层使用轴对齐文字框。输入最长边超过 1920 时缩小识别，小字宜缩小选区。复杂排版覆盖情况见 [验证](VERIFICATION.md)。

上游：[模型](https://rapidai.github.io/RapidOCRDocs/main/en/model_list/) · [安装](https://rapidai.github.io/RapidOCRDocs/main/en/install_usage/rapidocr/install/)。
