#!/usr/bin/env bash
set -euo pipefail
prefix="${MOKOU_INSTALL_PREFIX:-$HOME/.local}"
case "$prefix" in /*) ;; *) printf 'MOKOU_INSTALL_PREFIX 需要绝对路径。\n' >&2; exit 2 ;; esac
config_root="${XDG_CONFIG_HOME:-$HOME/.config}"
# Linux comm names are limited to 15 bytes.
process_name="mokou-screenshot"
if pgrep -u "$(id -u)" -x "${process_name:0:15}" >/dev/null; then
  printf '请先从托盘退出Mokou Screenshot，然后重新运行本脚本。\n' >&2
  exit 1
fi
rm -f -- "$prefix/bin/mokou-screenshot" "$prefix/share/applications/io.github.fedokou.MokouScreenshot.desktop" "$prefix/share/icons/hicolor/scalable/apps/mokou-screenshot.svg" "$config_root/autostart/io.github.fedokou.MokouScreenshot.desktop"
rm -f -- "$prefix/share/licenses/mokou-screenshot/LICENSE" \
  "$prefix/share/licenses/mokou-screenshot/THIRD_PARTY_NOTICES.md" \
  "$prefix/share/licenses/mokou-screenshot/LGPL-2.1-or-later.txt"
rmdir -- "$prefix/share/licenses/mokou-screenshot" 2>/dev/null || true
if command -v kbuildsycoca6 >/dev/null; then kbuildsycoca6 --noincremental >/dev/null 2>&1; fi
printf '已卸载。截图、录屏、设置和工程源码均保留。\n'
printf '本地 OCR 环境保留在 %s/share/mokou-screenshot/ocr，可手动清理以释放空间。\n' "$prefix"
printf '如需更改 PrtScn，请在 KDE 系统设置→快捷键中配置。\n'
