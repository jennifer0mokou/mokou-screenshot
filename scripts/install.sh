#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
prefix="${MUSHROOM_INSTALL_PREFIX:-$HOME/.local}"
config_root="${XDG_CONFIG_HOME:-$HOME/.config}"
with_ocr=true
case "${1:-}" in
  --without-ocr) with_ocr=false; shift ;;
  --help|-h) printf '用法：%s [--without-ocr]\n默认安装本地 OCR；仅使用视觉 AI 时可跳过。\n' "$0"; exit 0 ;;
  "") ;;
  *) printf '未知选项：%s\n' "$1" >&2; exit 2 ;;
esac
if (( $# )); then printf '参数过多。\n' >&2; exit 2; fi
case "$prefix" in /*) ;; *) printf 'MUSHROOM_INSTALL_PREFIX 需要绝对路径。\n' >&2; exit 2 ;; esac
if "$with_ocr" && ! command -v uv >/dev/null; then
  printf '本地 OCR 需要 uv：请先安装 uv，或使用 --without-ocr。\n' >&2
  exit 1
fi
if pgrep -u "$(id -u)" -x mushroom-shot >/dev/null; then
  printf '蘑菇截图仍在运行。请先从托盘退出，或运行 mushroom-shot --quit，待录屏保存完成后重试。\n' >&2
  exit 1
fi
cmake -S "$root" -B "$root/build" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX="$prefix"
cmake --build "$root/build" --parallel 4
if "$with_ocr"; then "$root/scripts/install-ocr.sh"; fi
cmake --install "$root/build"
mkdir -p "$config_root/autostart"
python3 - "$prefix" "$config_root/autostart/io.github.fedokou.MushroomShot.desktop" <<'AUTOSTART'
import pathlib,sys
prefix,out=sys.argv[1:]
pathlib.Path(out).write_text('[Desktop Entry]\nType=Application\nName=蘑菇截图\nExec="'+prefix+'/bin/mushroom-shot" --background\nIcon=mushroom-shot\nTerminal=false\nX-KDE-autostart-after=panel\n')
AUTOSTART
if command -v kbuildsycoca6 >/dev/null; then kbuildsycoca6 --noincremental >/dev/null 2>&1; fi
printf '已安装至 %s/bin/mushroom-shot；已启用登录后待命。\n' "$prefix"
printf '首次运行：%s/bin/mushroom-shot --settings\n' "$prefix"
