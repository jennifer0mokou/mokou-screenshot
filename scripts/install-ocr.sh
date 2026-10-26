#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
prefix="${MOKOU_INSTALL_PREFIX:-$HOME/.local}"
case "$prefix" in /*) ;; *) printf 'MOKOU_INSTALL_PREFIX 需要绝对路径。\n' >&2; exit 2 ;; esac
runtime="$prefix/share/mokou-screenshot/ocr"
if ! command -v uv >/dev/null; then
  printf '本地 OCR 安装需要 uv，请先安装 uv 再运行此脚本。\n' >&2
  exit 1
fi
mkdir -p "$runtime"
uv venv --python 3.12 --allow-existing "$runtime/venv"
uv pip sync --require-hashes --python "$runtime/venv/bin/python" "$root/ocr/requirements.lock"
"$runtime/venv/bin/python" "$root/ocr/setup_models.py" "$runtime"
install -m 644 "$root/ocr/worker.py" "$runtime/worker.py"
"$runtime/venv/bin/python" - <<'PY'
import hashlib,pathlib,rapidocr
models=pathlib.Path(rapidocr.__file__).parent/'models'
expected={
'PP-OCRv6_det_small.onnx':'090f04abcd9d9a7498bc4ebf677e4cb9bdce1fe4197ddb7e529f1ef44e1ff94f',
'PP-OCRv6_rec_small.onnx':'6f327246b50388f3c176ae304bd95767ea6dc0c9ae92153ef8cbe210b3c14884',
'ch_ppocr_mobile_v2.0_cls_mobile.onnx':'e47acedf663230f8863ff1ab0e64dd2d82b838fceb5957146dab185a89d6215c'}
for name,digest in expected.items():
    if hashlib.sha256((models/name).read_bytes()).hexdigest()!=digest:
        raise SystemExit('OCR 模型校验失败：'+name)
print('本地中英日／法韩俄 OCR 已安装；模型 SHA256 校验通过，识别不需要联网。')
PY
