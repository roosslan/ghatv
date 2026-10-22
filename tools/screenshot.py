#!/usr/bin/env python3
"""Снимок экрана ghatv в PNG или анимация в GIF.

Программа запускается в псевдотерминале, вывод разбирается эмулятором терминала pyte,
экран рисуется шрифтом DejaVu Sans Mono с палитрой VGA (как в Turbo Vision).

Кадр снимается после начального ожидания и после каждого --keys. Внутри одного --keys
нажатия разделяются символом «|» и отправляются с паузой. Для PNG сохраняется последний
кадр, для GIF — все кадры.

Запуск в образе ghatv:
    docker run --rm -e GH_TOKEN -e TZ -v "$PWD:/work" --entrypoint sh ghatv:latest -c \
      "apt-get update -qq && apt-get install -y -qq python3-pyte python3-pil python3-fonttools fonts-dejavu-core \
       && python3 /work/tools/screenshot.py --out /work/docs/screenshot.png"
"""

import argparse
import codecs
import fcntl
import os
import pty
import select
import signal
import struct
import termios
import time

import pyte
from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

FONT_MONO = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
FONT_SANS = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

# Палитра VGA в порядке BIOS-цветов ANSI: black, red, green, brown, blue, magenta, cyan, white
VGA = [
    (0x00, 0x00, 0x00), (0xAA, 0x00, 0x00), (0x00, 0xAA, 0x00), (0xAA, 0x55, 0x00),
    (0x00, 0x00, 0xAA), (0xAA, 0x00, 0xAA), (0x00, 0xAA, 0xAA), (0xAA, 0xAA, 0xAA),
    (0x55, 0x55, 0x55), (0xFF, 0x55, 0x55), (0x55, 0xFF, 0x55), (0xFF, 0xFF, 0x55),
    (0x55, 0x55, 0xFF), (0xFF, 0x55, 0xFF), (0x55, 0xFF, 0xFF), (0xFF, 0xFF, 0xFF),
]
NAMES = ["black", "red", "green", "brown", "blue", "magenta", "cyan", "white"]


class Screen(pyte.Screen):
    """pyte изображает яркие цвета 90-97 как «цвет + жирный» и не сбрасывает жирность
    у следующих тёмных цветов. Яркие коды переводятся в 256-цветную форму."""

    def select_graphic_rendition(self, *attrs, **kwargs):
        out = []
        for a in attrs:
            if 90 <= a <= 97:
                out += [38, 5, a - 90 + 8]
            elif 100 <= a <= 107:
                out += [48, 5, a - 100 + 8]
            else:
                out.append(a)
        super().select_graphic_rendition(*out, **kwargs)


def color_index(value, bold, is_fg):
    """Индекс VGA для цвета pyte или None, если это произвольный RGB."""
    if value == "default":
        return 7 if is_fg else 0
    name = value.replace("bright", "")
    if name == "yellow":
        name = "brown"
    if name in NAMES:
        idx = NAMES.index(name)
        if value.startswith("bright"):
            idx += 8
        return idx
    # 256-цветный режим: первые 16 цветов pyte отдаёт как hex из таблицы xterm
    xterm16 = [c.lower() for c in pyte.graphics.FG_BG_256[:16]]
    if value.lower() in xterm16:
        return xterm16.index(value.lower())
    return None


def to_rgb(value, bold, is_fg):
    idx = color_index(value, bold, is_fg)
    if idx is not None:
        return VGA[idx]
    try:
        return tuple(int(value[i:i + 2], 16) for i in (0, 2, 4))
    except ValueError:
        return VGA[7] if is_fg else VGA[0]


def run(args, on_frame):
    screen = Screen(args.cols, args.rows)
    stream = pyte.ByteStream(screen)

    pid, fd = pty.fork()
    if pid == 0:
        env = dict(os.environ, TERM="xterm-256color")
        env.pop("COLORTERM", None)
        os.execvpe(args.cmd, [args.cmd], env)

    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", args.rows, args.cols, 0, 0))

    def pump(seconds):
        end = time.time() + seconds
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.2)
            if r:
                try:
                    data = os.read(fd, 65536)
                except OSError:
                    return
                if not data:
                    return
                stream.feed(data)

    pump(args.wait)
    on_frame(screen)
    for keys in args.keys:
        for chunk in keys.split("|"):
            os.write(fd, codecs.decode(chunk, "unicode_escape").encode("utf-8"))
            pump(0.5)
        pump(args.key_wait)
        on_frame(screen)

    os.kill(pid, signal.SIGTERM)


def render(screen, args):
    mono = ImageFont.truetype(FONT_MONO, args.font_size)
    sans = ImageFont.truetype(FONT_SANS, args.font_size)
    mono_cmap = TTFont(FONT_MONO).getBestCmap()

    asc, desc = mono.getmetrics()
    cw = round(mono.getlength("M"))
    ch = asc + desc
    img = Image.new("RGB", (cw * screen.columns, ch * screen.lines), VGA[0])
    draw = ImageDraw.Draw(img)

    for y in range(screen.lines):
        line = screen.buffer[y]
        for x in range(screen.columns):
            c = line[x]
            fg = to_rgb(c.fg, c.bold, True)
            bg = to_rgb(c.bg, False, False)
            if c.reverse:
                fg, bg = bg, fg
            x0, y0 = x * cw, y * ch
            draw.rectangle([x0, y0, x0 + cw - 1, y0 + ch - 1], fill=bg)
            text = c.data
            if not text or text == " ":
                continue
            # Символы, которых нет в моноширинном шрифте (✔, ✘ и т. п.), берутся из DejaVu Sans
            font = mono if all(ord(t) in mono_cmap for t in text) else sans
            # Псевдографика рисуется прямоугольниками: так линии рамок не прерываются
            if not draw_box(draw, text, x0, y0, cw, ch, fg):
                draw.text((x0, y0), text, font=font, fill=fg)

    if args.scale != 1:
        img = img.resize((int(img.width * args.scale), int(img.height * args.scale)), Image.LANCZOS)
    return img


def save(frames, args):
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    if args.out.lower().endswith(".gif"):
        pal = [f.convert("P", palette=Image.ADAPTIVE, colors=256) for f in frames]
        # Первый кадр показывается дольше
        durations = [args.first_ms] + [args.frame_ms] * (len(pal) - 1)
        pal[0].save(args.out, save_all=True, append_images=pal[1:], duration=durations, loop=0, optimize=True)
    else:
        frames[-1].save(args.out, optimize=True)
    w, h = frames[-1].size
    print(f"{args.out}: {w}x{h}, кадров: {len(frames) if args.out.lower().endswith('.gif') else 1}")


# Линии рамок: (вверх, вниз, влево, вправо), 1 — одинарная, 2 — двойная
BOX = {
    "─": (0, 0, 1, 1), "│": (1, 1, 0, 0), "┌": (0, 1, 0, 1), "┐": (0, 1, 1, 0),
    "└": (1, 0, 0, 1), "┘": (1, 0, 1, 0), "├": (1, 1, 0, 1), "┤": (1, 1, 1, 0),
    "┬": (0, 1, 1, 1), "┴": (1, 0, 1, 1), "┼": (1, 1, 1, 1),
    "═": (0, 0, 2, 2), "║": (2, 2, 0, 0), "╔": (0, 2, 0, 2), "╗": (0, 2, 2, 0),
    "╚": (2, 0, 0, 2), "╝": (2, 0, 2, 0), "╟": (2, 2, 0, 1), "╢": (2, 2, 1, 0),
    "╤": (0, 1, 2, 2), "╧": (1, 0, 2, 2),
}


def draw_box(draw, text, x0, y0, cw, ch, fg):
    spec = BOX.get(text)
    if not spec:
        return False
    up, down, left, right = spec
    cx, cy = x0 + cw // 2, y0 + ch // 2
    gap = max(2, cw // 4)  # расстояние между линиями двойной рамки

    def hline(x1, x2, kind):
        if kind == 1:
            draw.line([(x1, cy), (x2, cy)], fill=fg)
        elif kind == 2:
            draw.line([(x1, cy - gap // 2), (x2, cy - gap // 2)], fill=fg)
            draw.line([(x1, cy + gap // 2), (x2, cy + gap // 2)], fill=fg)

    def vline(y1, y2, kind):
        if kind == 1:
            draw.line([(cx, y1), (cx, y2)], fill=fg)
        elif kind == 2:
            draw.line([(cx - gap // 2, y1), (cx - gap // 2, y2)], fill=fg)
            draw.line([(cx + gap // 2, y1), (cx + gap // 2, y2)], fill=fg)

    if left:
        hline(x0, cx, left)
    if right:
        hline(cx, x0 + cw - 1, right)
    if up:
        vline(y0, cy, up)
    if down:
        vline(cy, y0 + ch - 1, down)
    return True


def main():
    p = argparse.ArgumentParser(description="Снимок экрана ghatv в PNG")
    p.add_argument("--out", default="docs/screenshot.png")
    p.add_argument("--cmd", default="ghatv")
    p.add_argument("--cols", type=int, default=160)
    p.add_argument("--rows", type=int, default=42)
    p.add_argument("--wait", type=float, default=15, help="секунд до первого снимка (загрузка данных)")
    p.add_argument("--keys", action="append", default=[],
                   help="нажатия после ожидания, например '\\x1b2' (Alt+2); «|» разделяет нажатия; "
                        "после каждого --keys снимается кадр")
    p.add_argument("--key-wait", type=float, default=3)
    p.add_argument("--font-size", type=int, default=15)
    p.add_argument("--scale", type=float, default=1)
    p.add_argument("--frame-ms", type=int, default=2500, help="длительность кадра GIF, мс")
    p.add_argument("--first-ms", type=int, default=3500, help="длительность первого кадра GIF, мс")
    args = p.parse_args()
    frames = []
    run(args, lambda screen: frames.append(render(screen, args)))
    save(frames, args)


if __name__ == "__main__":
    main()
