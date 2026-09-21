#!/usr/bin/env bash
# ==============================================================================
# Generate Multilingual CJK (Chinese, Japanese, English) LVGL Binary Font Asset
# ==============================================================================
# This script converts Noto Sans CJK SC (SIL OFL 1.1) into an LVGL 9 binary font
# file (.bin) covering ASCII, Japanese Kana, CJK Punctuation, and CJK Ideographs.
#
# Supported modes:
#   compact : ~8,000 characters (Full GB2312 6,763 Hanzi + Japanese Joyo 2,135
#             + Kana + ASCII + Workflow Status Symbols). ~837 KB @ 4bpp.
#   full    : All 20,902 CJK Unified Ideographs (0x4E00-0x9FA5) + Kana + ASCII.
#             ~2.3 MB @ 4bpp.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Ensure Homebrew and standard tool paths are accessible
export PATH="/opt/homebrew/bin:/usr/local/bin:${PATH}"

FONT_DIR="${REPO_ROOT}/assets/fonts"
FONT_OTF="${FONT_DIR}/NotoSansCJKsc-Regular.otf"
SYMBOLS_TXT="${FONT_DIR}/compact_cjk_symbols.txt"

MODE="${1:-compact}"    # "compact" or "full"
FONT_SIZE="${2:-16}"    # 16 px
FONT_BPP="${3:-4}"      # 4 or 2 bpp

if [ "${MODE}" = "compact" ]; then
  DEFAULT_OUTPUT="${FONT_DIR}/noto_cjk_${FONT_SIZE}_compact_${FONT_BPP}bpp.bin"
else
  DEFAULT_OUTPUT="${FONT_DIR}/noto_cjk_${FONT_SIZE}_${FONT_BPP}bpp.bin"
fi

OUTPUT_FILE="${4:-${DEFAULT_OUTPUT}}"
FONT_URL="https://github.com/googlefonts/noto-cjk/raw/main/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf"

mkdir -p "${FONT_DIR}"

# 1. Ensure lv_font_conv is available
if ! command -v lv_font_conv &>/dev/null; then
  echo "Error: lv_font_conv is not installed or not in PATH."
  echo "Install it with: npm install -g lv_font_conv"
  exit 1
fi

# 2. Ensure Noto Sans CJK SC source font exists
if [ ! -f "${FONT_OTF}" ]; then
  echo "Downloading NotoSansCJKsc-Regular.otf from Google Fonts..."
  curl -fL -o "${FONT_OTF}" "${FONT_URL}"
fi

echo "======================================================================"
echo "Generating LVGL binary font:"
echo "  Mode:    ${MODE}"
echo "  Source:  ${FONT_OTF}"
echo "  Output:  ${OUTPUT_FILE}"
echo "  Size:    ${FONT_SIZE} px"
echo "  BPP:     ${FONT_BPP} (4bpp = 16-level grayscale, 2bpp = 4-level grayscale)"
echo "  Format:  bin (LVGL 9 lv_binfont / Flash partition)"
echo "======================================================================"

if [ "${MODE}" = "compact" ]; then
  if [ ! -f "${SYMBOLS_TXT}" ]; then
    echo "Error: ${SYMBOLS_TXT} not found."
    exit 1
  fi
  echo "Using compact symbol list (~4,785 essential CJK characters)..."
  lv_font_conv \
    --font "${FONT_OTF}" \
    --symbols "$(cat "${SYMBOLS_TXT}")" \
    --size "${FONT_SIZE}" \
    --bpp "${FONT_BPP}" \
    --format bin \
    --output "${OUTPUT_FILE}"
else
  echo "Using full Unicode CJK range (20,902 characters)..."
  UNICODE_RANGES="0x20-0x7E,0x00A0-0x00FF,0x2010-0x2027,0x2030-0x205E,0x20A0-0x20CF,0x2190-0x2193,0x221A,0x221E,0x2260,0x2264,0x2265,0x25A0-0x25FF,0x3000-0x303F,0x3040-0x309F,0x30A0-0x30FF,0xFF01-0xFF60,0x4E00-0x9FA5"
  lv_font_conv \
    --font "${FONT_OTF}" \
    -r "${UNICODE_RANGES}" \
    --size "${FONT_SIZE}" \
    --bpp "${FONT_BPP}" \
    --format bin \
    --output "${OUTPUT_FILE}"
fi

echo "======================================================================"
echo "Successfully generated font binary: ${OUTPUT_FILE}"
ls -lh "${OUTPUT_FILE}"
echo "======================================================================"
