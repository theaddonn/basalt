#!/usr/bin/env bash
# Regenerates the raster icons from assets/logo.svg.
# Needs rsvg-convert and ImageMagick.
set -euo pipefail
cd "$(dirname "$0")/.."

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

rsvg-convert -w 512 assets/logo.svg -o assets/logo.png
rsvg-convert -w 256 assets/logo.svg -o res/basalt.png

sizes=(16 24 32 48 64 128 256)
for s in "${sizes[@]}"; do
    rsvg-convert -w "$s" assets/logo.svg -o "$tmp/$s.png"
done
magick $(printf "$tmp/%s.png " "${sizes[@]}") res/basalt.ico
