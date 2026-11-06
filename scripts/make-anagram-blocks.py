"""Draws the 200x200 block images of the Anagram build (assets/anagram):
three offset parallel bars in the band colours on the dark background of the
editor; the "off" image is the same, dimmed. Needs Pillow; run once when the
design changes:  python3 scripts/make-anagram-blocks.py
"""
from pathlib import Path

from PIL import Image, ImageDraw

SIZE, SCALE = 200, 4                     # drawn at 4x, then reduced: smooth edges
BACKGROUND, BORDER = (11, 12, 16), (38, 42, 53)
BANDS = [(61, 220, 151), (255, 181, 71), (255, 92, 138)]   # low, mid, high


def block(on: bool) -> Image.Image:
    s = SIZE * SCALE
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, s - 1, s - 1], radius=36 * SCALE, fill=BACKGROUND,
                        outline=BORDER, width=3 * SCALE)
    for i, colour in enumerate(BANDS):
        x0, y = (40 + 16 * i) * SCALE, (62 + 34 * i) * SCALE
        rgb = colour if on else tuple(int(c * 0.3 + 30) for c in colour)
        d.rounded_rectangle([x0, y, x0 + 104 * SCALE, y + 14 * SCALE], radius=7 * SCALE, fill=rgb)
    return img.resize((SIZE, SIZE), Image.LANCZOS)


out = Path(__file__).resolve().parent.parent / "assets" / "anagram"
block(True).save(out / "block-on.png", optimize=True)
block(False).save(out / "block-off.png", optimize=True)
print(out / "block-on.png", out / "block-off.png")
