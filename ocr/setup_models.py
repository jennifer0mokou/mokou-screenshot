"""Install-time only: fetch and verify pinned optional recognition models."""

from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import sys
import urllib.request

root = Path(sys.argv[1])
manifest = json.loads((Path(__file__).parent / "extra-models.json").read_text())
(root / "models").mkdir(parents=True, exist_ok=True)


def install(item):
    name, metadata = item
    target = root / "models" / name
    if (
        target.is_file()
        and hashlib.sha256(target.read_bytes()).hexdigest() == metadata["sha256"]
    ):
        return
    temporary = target.with_suffix(".download")
    try:
        with (
            urllib.request.urlopen(metadata["url"], timeout=60) as response,
            temporary.open("wb") as out,
        ):
            while data := response.read(1024 * 1024):
                out.write(data)
        if hashlib.sha256(temporary.read_bytes()).hexdigest() != metadata["sha256"]:
            raise RuntimeError("OCR model checksum failed: " + name)
        temporary.replace(target)
    finally:
        temporary.unlink(missing_ok=True)


with ThreadPoolExecutor(max_workers=2) as executor:
    list(executor.map(install, manifest.items()))
print("韩语／俄语模型已下载并通过 SHA256 校验。")
