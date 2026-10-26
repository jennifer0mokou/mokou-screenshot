#!/usr/bin/env bash
set -euo pipefail
prefix="${MUSHROOM_INSTALL_PREFIX:-$HOME/.local}"
case "$prefix" in /*) ;; *) printf 'MUSHROOM_INSTALL_PREFIX 需要绝对路径。\n' >&2; exit 2 ;; esac
config_root="${XDG_CONFIG_HOME:-$HOME/.config}"
if pgrep -u "$(id -u)" -x mushroom-shot >/dev/null; then
  printf '请先从托盘退出蘑菇截图，然后重新运行本脚本。\n' >&2
  exit 1
fi
rm -f -- "$prefix/bin/mushroom-shot" "$prefix/share/applications/io.github.fedokou.MushroomShot.desktop" "$prefix/share/icons/hicolor/scalable/apps/mushroom-shot.svg" "$config_root/autostart/io.github.fedokou.MushroomShot.desktop"
rm -f -- "$prefix/share/licenses/mushroom-shot/LICENSE" \
  "$prefix/share/licenses/mushroom-shot/THIRD_PARTY_NOTICES.md" \
  "$prefix/share/licenses/mushroom-shot/LGPL-2.1-or-later.txt"
rmdir -- "$prefix/share/licenses/mushroom-shot" 2>/dev/null || true
if command -v kbuildsycoca6 >/dev/null; then kbuildsycoca6 --noincremental >/dev/null 2>&1; fi
printf '已卸载。截图、录屏、设置和工程源码均保留。\n'
printf '本地 OCR 环境保留在 %s/share/mushroom-shot/ocr，可手动清理以释放空间。\n' "$prefix"
printf '如需更改 PrtScn，请在 KDE 系统设置→快捷键中配置。\n'
