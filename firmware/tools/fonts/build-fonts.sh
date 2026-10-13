#!/usr/bin/env bash
# Gera as fontes Inter para o LVGL. Rode de dentro de firmware/tools/fonts/ depois do primeiro `pio run -e board`
# (o FontAwesome vem de dentro do pacote do LVGL). Precisa de node (npx). Os .c gerados são commitados.
set -euo pipefail

CACHE=.cache
OUT=../../src/theme
FA=../../.pio/libdeps/board/lvgl/scripts/built_in_font/FontAwesome5-Solid+Brands+Regular.woff

mkdir -p "$CACHE"
if [ ! -f "$CACHE/Inter-Regular.ttf" ]; then
    curl -sL -o "$CACHE/inter.zip" https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip
    unzip -o -j "$CACHE/inter.zip" 'extras/ttf/Inter-Regular.ttf' 'extras/ttf/Inter-Medium.ttf' -d "$CACHE"
fi

# Latin-1 completo cobre os acentos do português; meia-risca, travessão, bullet (LV_SYMBOL_BULLET) e reticências
# são tipográficos e vêm do Inter, não do FontAwesome.
# Os símbolos LVGL (LV_SYMBOL_*) entram só nas fontes de texto.
LATIN='0x20-0x7E,0xA0-0xFF,0x2013,0x2014,0x2022,0x2026'
SYMBOLS='0xF001,0xF008,0xF00B,0xF00C,0xF00D,0xF011,0xF013,0xF015,0xF019,0xF01C,0xF021,0xF026,0xF027,0xF028,0xF03E,0xF043,0xF048,0xF04B,0xF04C,0xF04D,0xF051,0xF052,0xF053,0xF054,0xF067,0xF068,0xF06E,0xF070,0xF071,0xF074,0xF077,0xF078,0xF079,0xF07B,0xF093,0xF095,0xF0C4,0xF0C5,0xF0C7,0xF0C9,0xF0E0,0xF0E7,0xF0EA,0xF0F3,0xF11C,0xF124,0xF158,0xF1EB,0xF240,0xF241,0xF242,0xF243,0xF244,0xF287,0xF293,0xF2ED,0xF304,0xF55A,0xF7C2,0xF8A2'

text_font() {
    local size=$1 weight=$2 name=$3
    npx --yes lv_font_conv@1.5.3 --bpp 4 --size "$size" --format lvgl --no-compress --lv-include lvgl.h \
        --font "$CACHE/Inter-$weight.ttf" --range "$LATIN" \
        --font "$FA" --range "$SYMBOLS" \
        -o "$OUT/$name.c"
}

text_font 12 Regular inter_12
text_font 14 Regular inter_14
text_font 18 Medium inter_18
text_font 24 Medium inter_24

npx --yes lv_font_conv@1.5.3 --bpp 4 --size 64 --format lvgl --no-compress --lv-include lvgl.h \
    --font "$CACHE/Inter-Medium.ttf" --range '0x30-0x3A' -o "$OUT/inter_64_digits.c"
