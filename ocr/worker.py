#!/usr/bin/env python3
"""Local-only OCR worker. JSON lines on pipes; never persists images or OCR text."""

import base64
import contextlib
import io
import json
import os
from pathlib import Path
import sys
import time

MAX_REQUEST = 64 * 1024 * 1024
protocol = sys.stdout
engine = None
engine_language = None


def recognize(payload):
    global engine, engine_language
    import numpy as np
    from PIL import Image

    language = payload.get("language", "multi")
    if language not in ("multi", "french", "korean", "russian"):
        raise ValueError("unsupported_language")
    if engine is None or engine_language != language:
        engine = None
        from rapidocr import ModelType, OCRVersion, RapidOCR
        import rapidocr

        # All three files ship in the pinned wheel. No first-use model download.
        models = Path(rapidocr.__file__).parent / "models"
        paths = {
            "Det": "PP-OCRv6_det_small.onnx",
            "Rec": "PP-OCRv6_rec_small.onnx",
            "Cls": "ch_ppocr_mobile_v2.0_cls_mobile.onnx",
        }
        if not all((models / name).is_file() for name in paths.values()):
            raise RuntimeError("models_missing")
        import requests

        def offline(*args, **kwargs):
            raise RuntimeError("runtime_network_disabled")

        requests.sessions.Session.request = offline
        params = {
            f"{module}.model_path": str(models / name) for module, name in paths.items()
        }
        params.update(
            {
                "Global.log_level": "critical",
                "Det.limit_type": "max",
                "Det.limit_side_len": 1920,
                "EngineConfig.onnxruntime.intra_op_num_threads": 4,
                "EngineConfig.onnxruntime.inter_op_num_threads": 1,
            }
        )
        if language in ("korean", "russian"):
            route = "korean" if language == "korean" else "eslav"
            model = (
                Path(__file__).parent / "models" / f"{route}_PP-OCRv5_rec_mobile.onnx"
            )
            if not model.is_file():
                raise RuntimeError("models_missing")
            params.update(
                {
                    "Rec.model_path": str(model),
                    "Rec.lang_type": route,
                    "Rec.ocr_version": OCRVersion.PPOCRV5,
                    "Rec.model_type": ModelType.MOBILE,
                }
            )
        elif language == "french":
            params["Rec.lang_type"] = "french"
        engine = RapidOCR(params=params)
        engine_language = language
    raw = base64.b64decode(payload["png"], validate=True)
    image = Image.open(io.BytesIO(raw))
    if image.width * image.height > 100_000_000:
        raise ValueError("image_too_large")
    # RapidOCR's numpy input uses OpenCV BGR order.
    array = np.asarray(image.convert("RGB"))[:, :, ::-1].copy()
    result = engine(array)
    blocks = []
    if result.txts is not None:
        for points, text in zip(result.boxes, result.txts):
            if not text.strip():
                continue
            x0 = max(0.0, float(points[:, 0].min())) / image.width
            y0 = max(0.0, float(points[:, 1].min())) / image.height
            x1 = min(float(image.width), float(points[:, 0].max())) / image.width
            y1 = min(float(image.height), float(points[:, 1].max())) / image.height
            if x1 > x0 and y1 > y0:
                blocks.append({"text": text, "bbox": [x0, y0, x1 - x0, y1 - y0]})
    return blocks


with open(os.devnull, "w") as quiet:
    while True:
        line = sys.stdin.buffer.readline(MAX_REQUEST + 1)
        if not line:
            break
        if len(line) > MAX_REQUEST:
            break
        started = time.perf_counter()
        try:
            with contextlib.redirect_stdout(quiet), contextlib.redirect_stderr(quiet):
                payload = json.loads(line)
                blocks = recognize(payload)
            response = {
                "coordinate_system": "normalized",
                "box_format": "xywh",
                "blocks": blocks,
                "elapsed_ms": round((time.perf_counter() - started) * 1000),
            }
        except Exception:
            # No exception text: it can contain image contents or library diagnostics.
            response = {"error": "local_ocr_failed"}
        protocol.write(
            json.dumps(response, ensure_ascii=False, separators=(",", ":")) + "\n"
        )
        protocol.flush()
        # Keep model weights warm, release request data before waiting for another image.
        line = payload = blocks = response = None
