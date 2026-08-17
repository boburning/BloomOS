#!/usr/bin/env python3
"""Generate the BloomOS Quick Guide PNG pages without external dependencies."""

import argparse
import binascii
import pathlib
import struct
import zlib


WIDTH = 640
HEIGHT = 480
PALETTE = (
    (13, 18, 28),
    (24, 32, 47),
    (109, 214, 170),
    (123, 174, 255),
    (239, 244, 250),
    (165, 178, 196),
)
BACKGROUND, PANEL, ACCENT, ACCENT_2, TEXT, MUTED = range(len(PALETTE))

# Public-domain-style 5x7 bitmap glyph definitions authored for BloomOS.
FONT = {
    " ": (0, 0, 0, 0, 0, 0, 0),
    "A": (14, 17, 17, 31, 17, 17, 17), "B": (30, 17, 17, 30, 17, 17, 30),
    "C": (14, 17, 16, 16, 16, 17, 14), "D": (30, 17, 17, 17, 17, 17, 30),
    "E": (31, 16, 16, 30, 16, 16, 31), "F": (31, 16, 16, 30, 16, 16, 16),
    "G": (14, 17, 16, 23, 17, 17, 15), "H": (17, 17, 17, 31, 17, 17, 17),
    "I": (14, 4, 4, 4, 4, 4, 14), "J": (7, 2, 2, 2, 18, 18, 12),
    "K": (17, 18, 20, 24, 20, 18, 17), "L": (16, 16, 16, 16, 16, 16, 31),
    "M": (17, 27, 21, 21, 17, 17, 17), "N": (17, 25, 21, 19, 17, 17, 17),
    "O": (14, 17, 17, 17, 17, 17, 14), "P": (30, 17, 17, 30, 16, 16, 16),
    "Q": (14, 17, 17, 17, 21, 18, 13), "R": (30, 17, 17, 30, 20, 18, 17),
    "S": (15, 16, 16, 14, 1, 1, 30), "T": (31, 4, 4, 4, 4, 4, 4),
    "U": (17, 17, 17, 17, 17, 17, 14), "V": (17, 17, 17, 17, 17, 10, 4),
    "W": (17, 17, 17, 21, 21, 21, 10), "X": (17, 17, 10, 4, 10, 17, 17),
    "Y": (17, 17, 10, 4, 4, 4, 4), "Z": (31, 1, 2, 4, 8, 16, 31),
    "0": (14, 17, 19, 21, 25, 17, 14), "1": (4, 12, 4, 4, 4, 4, 14),
    "2": (14, 17, 1, 2, 4, 8, 31), "3": (30, 1, 1, 14, 1, 1, 30),
    "4": (2, 6, 10, 18, 31, 2, 2), "5": (31, 16, 16, 30, 1, 1, 30),
    "6": (14, 16, 16, 30, 17, 17, 14), "7": (31, 1, 2, 4, 8, 8, 8),
    "8": (14, 17, 17, 14, 17, 17, 14), "9": (14, 17, 17, 15, 1, 1, 14),
    ".": (0, 0, 0, 0, 0, 6, 6), ",": (0, 0, 0, 0, 6, 6, 4),
    ":": (0, 6, 6, 0, 6, 6, 0), "/": (1, 2, 4, 8, 16, 0, 0),
    "-": (0, 0, 0, 31, 0, 0, 0), "+": (0, 4, 4, 31, 4, 4, 0),
    "!": (4, 4, 4, 4, 4, 0, 4), "?": (14, 17, 1, 2, 4, 0, 4),
    "'": (4, 4, 0, 0, 0, 0, 0), "&": (12, 18, 20, 8, 21, 18, 13),
    "(": (2, 4, 8, 8, 8, 4, 2), ")": (8, 4, 2, 2, 2, 4, 8),
}


class Canvas:
    def __init__(self):
        self.pixels = bytearray([BACKGROUND]) * (WIDTH * HEIGHT)

    def rect(self, x, y, width, height, color):
        for row in range(max(0, y), min(HEIGHT, y + height)):
            start = row * WIDTH + max(0, x)
            end = row * WIDTH + min(WIDTH, x + width)
            self.pixels[start:end] = bytes([color]) * (end - start)

    def text(self, x, y, value, scale=3, color=TEXT):
        cursor = x
        for character in value.upper():
            glyph = FONT.get(character, FONT["?"])
            for row, bits in enumerate(glyph):
                for column in range(5):
                    if bits & (1 << (4 - column)):
                        self.rect(cursor + column * scale, y + row * scale, scale, scale, color)
            cursor += 6 * scale

    def write_png(self, path):
        raw = b"".join(
            b"\x00" + self.pixels[row * WIDTH:(row + 1) * WIDTH]
            for row in range(HEIGHT)
        )
        def chunk(kind, data):
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", binascii.crc32(kind + data) & 0xFFFFFFFF)
        png = b"\x89PNG\r\n\x1a\n"
        png += chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 3, 0, 0, 0))
        png += chunk(b"PLTE", bytes(channel for color in PALETTE for channel in color))
        # Stored DEFLATE blocks avoid zlib-version-dependent compression choices,
        # making checked-in PNGs byte-identical on Windows, Linux, and CI.
        compressed = bytearray(b"\x78\x01")
        for offset in range(0, len(raw), 65535):
            block = raw[offset:offset + 65535]
            final = offset + len(block) == len(raw)
            compressed.append(1 if final else 0)
            compressed.extend(struct.pack("<HH", len(block), 0xFFFF - len(block)))
            compressed.extend(block)
        compressed.extend(struct.pack(">I", zlib.adler32(raw) & 0xFFFFFFFF))
        png += chunk(b"IDAT", bytes(compressed))
        png += chunk(b"IEND", b"")
        path.write_bytes(png)


def header(canvas, section, title):
    canvas.rect(0, 0, WIDTH, 8, ACCENT)
    canvas.text(36, 28, "BLOOMOS", 3, ACCENT)
    canvas.text(500, 30, section, 2, MUTED)
    canvas.text(36, 82, title, 5, TEXT)


def line(canvas, y, label, value, color=ACCENT_2):
    canvas.rect(38, y + 5, 8, 8, ACCENT)
    canvas.text(60, y, label, 3, color)
    canvas.text(60, y + 31, value, 2, MUTED)


def page_one():
    canvas = Canvas()
    header(canvas, "01 / 04", "QUICK GUIDE")
    canvas.text(38, 142, "START HERE", 3, ACCENT_2)
    line(canvas, 190, "PLAY", "CHOOSE A SYSTEM, THEN SELECT A GAME")
    line(canvas, 262, "PAUSE", "PRESS MENU TO OPEN THE IN-GAME MENU")
    line(canvas, 334, "POWER", "USE THE MAIN MENU SHUTDOWN COMMAND")
    canvas.text(38, 440, "PRESS LEFT OR RIGHT FOR MORE", 2, MUTED)
    return canvas


def page_two():
    canvas = Canvas()
    header(canvas, "02 / 04", "FEATURES")
    line(canvas, 146, "RECENTS", "CONTINUE GAMES FROM WHERE YOU LEFT OFF")
    line(canvas, 218, "FAVORITES", "KEEP YOUR MOST-PLAYED GAMES TOGETHER")
    line(canvas, 290, "ACTIVITY", "VIEW PLAY TIME AND SESSION HISTORY")
    line(canvas, 362, "APPS", "TOOLS, SETTINGS, FILES, AND QUICK GUIDE")
    return canvas


def button(canvas, x, y, label, description):
    canvas.rect(x, y, 72, 44, PANEL)
    canvas.rect(x, y, 72, 3, ACCENT)
    scale = 2 if len(label) > 2 else 3
    canvas.text(x + 10, y + 11, label, scale, TEXT)
    canvas.text(x + 94, y + 5, description, 2, MUTED)


def page_three():
    canvas = Canvas()
    header(canvas, "03 / 04", "SHORTCUTS")
    canvas.text(38, 142, "HOLD MENU, THEN PRESS", 3, ACCENT_2)
    button(canvas, 38, 194, "R2", "SAVE STATE")
    button(canvas, 38, 254, "L2", "LOAD STATE")
    button(canvas, 38, 314, "R", "FAST FORWARD")
    button(canvas, 330, 194, "X", "FPS DISPLAY")
    button(canvas, 330, 254, "SEL", "QUICK MENU")
    button(canvas, 330, 314, "L", "REWIND")
    canvas.text(38, 414, "PRESS MENU: SAVE AND EXIT", 3, ACCENT)
    return canvas


def page_four():
    canvas = Canvas()
    header(canvas, "04 / 04", "HELP & SAFETY")
    line(canvas, 150, "SHUTDOWN", "AVOID REMOVING POWER WHILE BLOOMOS RUNS")
    line(canvas, 230, "STORAGE", "EJECT THE SD CARD SAFELY FROM YOUR PC")
    line(canvas, 310, "SUPPORT", "GITHUB.COM/BOBURNING/BLOOMOS")
    canvas.rect(36, 398, 568, 44, PANEL)
    canvas.text(70, 410, "BUILT FOR MIYOO MINI, PLUS, AND FLIP", 2, ACCENT)
    return canvas


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for number, canvas in enumerate((page_one(), page_two(), page_three(), page_four()), 1):
        canvas.write_png(args.output / f"page{number}.png")


if __name__ == "__main__":
    main()
