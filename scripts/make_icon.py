"""Generate assets/claudeexplorer.ico.

The mark is what the product draws: one session and the work hanging off it.
A filled disc in the middle, three satellites on links, in the ARCHON /
Specter Point red on black. It is drawn geometrically at every size rather
than downscaled from one large bitmap, because the taskbar renders this at
16px and a resampled cluster turns to grey soup there.

The ICO container is written by hand. Pillow's ICO writer resamples a single
source image to every requested size, which is exactly the behaviour this
script exists to avoid. Entries are PNG-compressed at all sizes, which Windows
has accepted since Vista and this product requires Windows 11 anyway.
"""

import io
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw

BLACK = (0, 0, 0, 255)
BORDER = (31, 31, 31, 255)  # #1F1F1F hairline
LINK = (74, 74, 74, 255)  # #4A4A4A, the same weight a link has on the canvas
RED = (255, 0, 0, 255)  # #FF0000, the only accent

SIZES = [16, 24, 32, 48, 64, 128, 256]

# Where the three satellites sit, in degrees clockwise from straight up. Not
# evenly spaced: three nodes at 120 degrees reads as a radiation trefoil, and
# the graph this stands for never settles into anything that regular.
ANGLES = [-64.0, 48.0, 168.0]


def disc(d: ImageDraw.ImageDraw, cx: float, cy: float, r: float, fill, outline=None,
         width: int = 1) -> None:
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=fill, outline=outline, width=width)


ORBIT = 0.33  # how far the satellites sit from the middle
HUB = 0.20  # the session
SAT = 0.085  # one agent


def render(s: int) -> Image.Image:
    # 16px is hinted rather than supersampled, and loses the frame.
    #
    # Both are the same decision: at 16 there are fourteen usable pixels after
    # a border, and four shapes have to survive in them. Antialiasing spends
    # those pixels on grey, and the frame spends two more on a #1F1F1F hairline
    # nothing can see at that size. Drawn straight onto whole pixels with the
    # border dropped, the ring and its three nodes still read.
    small = s <= 16
    scale = 1 if small else 4
    n = s * scale
    img = Image.new("RGBA", (n, n), BLACK)
    d = ImageDraw.Draw(img)

    if not small:
        d.rectangle([0, 0, n - 1, n - 1], outline=BORDER, width=scale)

    cx = cy = n / 2.0
    orbit, hub, sat = n * ORBIT, n * HUB, n * SAT
    link_w = max(1, round(n / (16 if small else 22)))
    ring_w = max(1, round(n / (16 if small else 20)))

    # Links first, so a disc is drawn over its own link rather than beside it.
    for deg in ANGLES:
        a = math.radians(deg - 90.0)
        d.line(
            [cx, cy, cx + orbit * math.cos(a), cy + orbit * math.sin(a)],
            fill=LINK,
            width=link_w,
        )

    for deg in ANGLES:
        a = math.radians(deg - 90.0)
        disc(d, cx + orbit * math.cos(a), cy + orbit * math.sin(a), sat, RED)

    # The hub is hollow: it is the thing the rest hangs off, not the loudest
    # thing in the frame, and a solid centre at 16px swallows its own links.
    disc(d, cx, cy, hub, BLACK, outline=RED, width=ring_w)

    return img.resize((s, s), Image.LANCZOS) if scale > 1 else img


def write_ico(path: Path, images: list[Image.Image]) -> None:
    blobs = []
    for im in images:
        buf = io.BytesIO()
        im.save(buf, format="PNG", optimize=True)
        blobs.append(buf.getvalue())

    n = len(blobs)
    header = struct.pack("<HHH", 0, 1, n)  # reserved, type=icon, count
    offset = len(header) + 16 * n

    entries = bytearray()
    for im, blob in zip(images, blobs):
        # A dimension of 256 is stored as 0.
        w = 0 if im.width >= 256 else im.width
        h = 0 if im.height >= 256 else im.height
        entries += struct.pack("<BBBBHHII", w, h, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)

    path.write_bytes(header + bytes(entries) + b"".join(blobs))


def main() -> None:
    out = Path(__file__).resolve().parent.parent / "assets" / "claudeexplorer.ico"
    out.parent.mkdir(parents=True, exist_ok=True)

    write_ico(out, [render(s) for s in SIZES])
    print(f"wrote {out} with {len(SIZES)} images: {SIZES}")


if __name__ == "__main__":
    main()
