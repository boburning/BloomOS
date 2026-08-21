#!/usr/bin/env python3
"""Generate Bloom-owned branding assets with only Python's standard library."""

from __future__ import annotations

import argparse
import binascii
import json
import pathlib
import struct
import zlib

from generate_quick_guide import FONT as BITMAP_FONT


GLYPHS = {
    "B": ("11110", "10001", "10001", "11110", "10001", "10001", "11110"),
    "L": ("10000", "10000", "10000", "10000", "10000", "10000", "11111"),
    "M": ("10001", "11011", "10101", "10101", "10001", "10001", "10001"),
    "O": ("01110", "10001", "10001", "10001", "10001", "10001", "01110"),
    "S": ("01111", "10000", "10000", "01110", "00001", "00001", "11110"),
}

DIRECTIONS = (
    (0, -1000),
    (707, -707),
    (1000, 0),
    (707, 707),
    (0, 1000),
    (-707, 707),
    (-1000, 0),
    (-707, -707),
)


def parse_color(value: str) -> tuple[int, int, int, int]:
    if len(value) != 7 or not value.startswith("#"):
        raise ValueError(f"invalid color: {value}")
    return int(value[1:3], 16), int(value[3:5], 16), int(value[5:7], 16), 255


class Canvas:
    def __init__(self, width: int, height: int, color: tuple[int, int, int, int]):
        self.width = width
        self.height = height
        self.pixels = bytearray(color * (width * height))

    def set(self, x: int, y: int, color: tuple[int, int, int, int]) -> None:
        if 0 <= x < self.width and 0 <= y < self.height:
            offset = (y * self.width + x) * 4
            self.pixels[offset : offset + 4] = bytes(color)

    def rectangle(self, x: int, y: int, width: int, height: int, color: tuple[int, int, int, int]) -> None:
        for py in range(y, y + height):
            for px in range(x, x + width):
                self.set(px, py, color)

    def ellipse(
        self,
        center_x: int,
        center_y: int,
        radius_long: int,
        radius_short: int,
        direction_x: int,
        direction_y: int,
        color: tuple[int, int, int, int],
    ) -> None:
        extent = radius_long + 2
        long_limit = radius_long * radius_long
        short_limit = radius_short * radius_short
        combined = long_limit * short_limit
        for py in range(center_y - extent, center_y + extent + 1):
            for px in range(center_x - extent, center_x + extent + 1):
                relative_x = px - center_x
                relative_y = py - center_y
                longitudinal = (relative_x * direction_x + relative_y * direction_y) // 1000
                lateral = (-relative_x * direction_y + relative_y * direction_x) // 1000
                if longitudinal * longitudinal * short_limit + lateral * lateral * long_limit <= combined:
                    self.set(px, py, color)

    def diamond(self, center_x: int, center_y: int, radius: int, color: tuple[int, int, int, int]) -> None:
        for py in range(center_y - radius, center_y + radius + 1):
            for px in range(center_x - radius, center_x + radius + 1):
                if abs(px - center_x) + abs(py - center_y) <= radius:
                    self.set(px, py, color)

    def text(self, x: int, y: int, value: str, scale: int, color: tuple[int, int, int, int]) -> None:
        cursor = x
        for character in value.upper():
            glyph = BITMAP_FONT.get(character, BITMAP_FONT["?"])
            for row, bits in enumerate(glyph):
                for column in range(5):
                    if bits & (1 << (4 - column)):
                        self.rectangle(cursor + column * scale, y + row * scale, scale, scale, color)
            cursor += 6 * scale

    def png(self) -> bytes:
        raw = bytearray()
        row_size = self.width * 4
        for row in range(self.height):
            raw.append(0)
            start = row * row_size
            raw.extend(self.pixels[start : start + row_size])

        def chunk(kind: bytes, data: bytes) -> bytes:
            payload = kind + data
            return struct.pack(">I", len(data)) + payload + struct.pack(">I", binascii.crc32(payload) & 0xFFFFFFFF)

        return (
            b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", self.width, self.height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + chunk(b"IEND", b"")
        )


def png_pixels(content: bytes) -> tuple[int, int, bytes]:
    if not content.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError("invalid PNG signature")
    offset = 8
    width = height = 0
    compressed = bytearray()
    while offset < len(content):
        length = struct.unpack(">I", content[offset : offset + 4])[0]
        kind = content[offset + 4 : offset + 8]
        data = content[offset + 8 : offset + 8 + length]
        offset += 12 + length
        if kind == b"IHDR":
            width, height, depth, color_type, compression, filtering, interlace = struct.unpack(">IIBBBBB", data)
            if (depth, color_type, compression, filtering, interlace) != (8, 6, 0, 0, 0):
                raise ValueError("unsupported PNG format")
        elif kind == b"IDAT":
            compressed.extend(data)
        elif kind == b"IEND":
            break
    raw = zlib.decompress(bytes(compressed))
    row_size = width * 4
    if len(raw) != height * (row_size + 1):
        raise ValueError("invalid PNG payload size")
    pixels = bytearray()
    for row in range(height):
        start = row * (row_size + 1)
        if raw[start] != 0:
            raise ValueError("unsupported PNG row filter")
        pixels.extend(raw[start + 1 : start + row_size + 1])
    return width, height, bytes(pixels)


def draw_mark(canvas: Canvas, center_x: int, center_y: int, size: int, colors: dict[str, tuple[int, int, int, int]]) -> None:
    petal_distance = max(2, size * 19 // 100)
    long_radius = max(2, size * 27 // 100)
    short_radius = max(1, size * 13 // 100)
    for index, (direction_x, direction_y) in enumerate(DIRECTIONS):
        petal_x = center_x + direction_x * petal_distance // 1000
        petal_y = center_y + direction_y * petal_distance // 1000
        canvas.ellipse(petal_x, petal_y, long_radius, short_radius, direction_x, direction_y, colors["orange"])
    canvas.diamond(center_x, center_y, max(2, size * 21 // 100), colors["gold"])
    center_radius = max(1, size * 12 // 100)
    canvas.ellipse(center_x, center_y, center_radius, center_radius, 1000, 0, colors["cream"])
    inner_radius = max(1, size * 6 // 100)
    canvas.ellipse(center_x, center_y, inner_radius, inner_radius, 1000, 0, colors["surface"])


def draw_wordmark(canvas: Canvas, x: int, y: int, scale: int, colors: dict[str, tuple[int, int, int, int]]) -> None:
    cursor = x
    for index, character in enumerate("BLOOMOS"):
        color = colors["cream"] if index < 5 else colors["gold"]
        for row, pattern in enumerate(GLYPHS[character]):
            for column, enabled in enumerate(pattern):
                if enabled == "1":
                    canvas.rectangle(cursor + column * scale, y + row * scale, scale, scale, color)
        cursor += 6 * scale


INSTALL_SLIDES = (
    ("WELCOME TO BLOOMOS", "A CLEAN START FOR YOUR", "MIYOO HANDHELD"),
    ("YOUR GAMES, YOUR CARD", "ROMS AND SAVES STAY IN", "FAMILIAR FOLDERS"),
    ("SAFE BY DEFAULT", "SIGNED UPDATES AND", "RECOVERABLE ROLLBACKS"),
    ("QUICK RETURN", "GAMESWITCHER KEEPS", "RECENT SESSIONS CLOSE"),
    ("PLAY ACTIVITY", "LOCAL HISTORY USES", "STABLE GAME IDENTITY"),
    ("RETROACHIEVEMENTS", "OPTIONAL, PRIVATE, AND", "BUILT AROUND EXACT GAMES"),
    ("OFFLINE FRIENDLY", "BADGES AND METADATA", "REMAIN AVAILABLE"),
    ("ONE BLOOMOS FAMILY", "MINI, MINI PLUS, AND FLIP", "SHARE THE SAME FOUNDATION"),
    ("READY TO PLAY", "INSTALLATION WILL FINISH", "AUTOMATICALLY"),
)


def install_slide(
    index: int,
    title: str,
    first_line: str,
    second_line: str,
    colors: dict[str, tuple[int, int, int, int]],
) -> bytes:
    slide = Canvas(640, 480, colors["canvas"])
    slide.rectangle(0, 0, 640, 8, colors["orange"])
    draw_mark(slide, 70, 70, 72, colors)
    slide.text(122, 43, "BLOOMOS", 3, colors["cream"])
    slide.rectangle(52, 132, 536, 4, colors["surface-raised"])
    slide.text(52, 170, title, 4, colors["cream"])
    slide.text(52, 265, first_line, 3, colors["sand"])
    slide.text(52, 310, second_line, 3, colors["sand"])
    slide.rectangle(52, 398, 536, 2, colors["surface-raised"])
    slide.text(52, 426, f"{index + 1:02d} / {len(INSTALL_SLIDES):02d}", 2, colors["gold"])
    slide.text(406, 426, "INSTALLING BLOOMOS", 2, colors["orange"])
    return slide.png()


def install_waiting(colors: dict[str, tuple[int, int, int, int]]) -> bytes:
    waiting = Canvas(640, 480, colors["canvas"])
    waiting.rectangle(0, 0, 640, 8, colors["orange"])
    draw_mark(waiting, 320, 190, 150, colors)
    draw_wordmark(waiting, 173, 292, 7, colors)
    waiting.text(230, 384, "PREPARING INSTALL", 2, colors["sand"])
    return waiting.png()


def generate(repository: pathlib.Path) -> dict[pathlib.Path, bytes]:
    tokens_path = repository / "build/bloom-design-tokens.json"
    mark_path = repository / "src/bloomUi/assets/bloom-mark.svg"
    tokens = json.loads(tokens_path.read_text(encoding="utf-8"))
    colors = {name: parse_color(value) for name, value in tokens["palette"].items()}
    mark = mark_path.read_text(encoding="utf-8")
    required_mark_colors = (tokens["palette"][name] for name in ("orange", "gold", "cream", "surface-raised"))
    if mark.count("<path") < 9 or any(color not in mark for color in required_mark_colors):
        raise ValueError("canonical Bloom mark is incompatible")

    outputs: dict[pathlib.Path, bytes] = {}
    for size in (24, 48):
        icon = Canvas(size, size, (0, 0, 0, 0))
        draw_mark(icon, size // 2, size // 2, size * 9 // 10, colors)
        outputs[repository / f"assets/branding/bloom-mark-{size}.png"] = icon.png()

    boot = Canvas(640, 480, colors["canvas"])
    draw_mark(boot, 142, 240, 112, colors)
    draw_wordmark(boot, 224, 216, 7, colors)
    boot_png = boot.png()
    outputs[repository / "assets/branding/bloom-boot-640x480.png"] = boot_png
    outputs[repository / "src/bootScreen/res/bootScreen.png"] = boot_png
    outputs[repository / "src/installUI/res/waitingBG.png"] = install_waiting(colors)
    for index, (title, first_line, second_line) in enumerate(INSTALL_SLIDES):
        outputs[repository / f"src/installUI/res/installSlide{index}.png"] = install_slide(
            index, title, first_line, second_line, colors
        )
    return outputs


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", type=pathlib.Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    repository = args.repository.resolve()
    stale: list[str] = []
    try:
        outputs = generate(repository)
    except (KeyError, OSError, UnicodeError, ValueError, json.JSONDecodeError) as error:
        print(f"Bloom branding source is invalid: {error}")
        return 1
    for path, content in outputs.items():
        if args.check:
            try:
                matches = path.is_file() and png_pixels(path.read_bytes()) == png_pixels(content)
            except (OSError, ValueError, zlib.error):
                matches = False
            if not matches:
                stale.append(path.relative_to(repository).as_posix())
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
    if stale:
        for path in stale:
            print(f"Bloom branding asset is stale: {path}")
        return 1
    print("Bloom branding assets: reproducible")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
