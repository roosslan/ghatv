#!/usr/bin/env python3
"""Значок ghatv для вкладки браузера: синий квадрат в цветах Turbo Vision
с бирюзовой рамкой и жёлтым треугольником «запуск».

    python3 tools/favicon.py assets/favicon.png
"""

import sys

from PIL import Image, ImageDraw

SIZE = 64
S = 4  # рисование с запасом и уменьшение даёт сглаженные края

BLUE = (0x00, 0x00, 0xAA, 255)
CYAN = (0x55, 0xFF, 0xFF, 255)
YELLOW = (0xFF, 0xFF, 0x55, 255)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "assets/favicon.png"
    n = SIZE * S
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    d.rounded_rectangle([0, 0, n - 1, n - 1], radius=10 * S, fill=BLUE)
    # Рамка окна Turbo Vision
    inset = 6 * S
    d.rounded_rectangle([inset, inset, n - 1 - inset, n - 1 - inset], radius=4 * S, outline=CYAN, width=3 * S)
    # Треугольник «запуск», смещён вправо для оптического центра
    cx, cy, h = n // 2 + 3 * S, n // 2, 30 * S
    d.polygon([(cx - h // 2, cy - h // 2), (cx - h // 2, cy + h // 2), (cx + h // 2, cy)], fill=YELLOW)

    img.resize((SIZE, SIZE), Image.LANCZOS).save(out, optimize=True)
    print(out)


if __name__ == "__main__":
    main()
