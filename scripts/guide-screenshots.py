#!/usr/bin/env python3
"""Grabs the app headlessly and writes the guide's annotated screenshots.

    scripts/guide-screenshots.py            # writes screenshots/guide/*.png

Each shot runs the built app (scripts/dev-run.sh) with a PLDL_DEBUG_OPEN hook
against a scratch profile, takes PLDL_DEBUG_GRAB's picture and its .json
sidecar (the named widgets' rectangles), then draws numbered callouts on the
controls the guide talks about. The legend for each number is in GUIDE.md.
"""
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "screenshots" / "guide"
FONT_BOLD = "/usr/share/fonts/ubuntu/Ubuntu-B.ttf"
ACCENT = (0xF6, 0xD3, 0x2D)
INK = (0x1C, 0x10, 0x26)
SIZE = "1280x800"

# name: (hook, dialog?, [(objectName, anchor), ...]); anchor: where the badge sits on the rect.
PLAN = {
    "window": ("search", False, [("railSearch", "r"), ("railPlaylist", "r"), ("railBrowser", "r"), ("railDownloads", "r"),
                                 ("railSettings", "r"), ("railAccount", "r"), ("helpButton", "r")]),
    "start": ("search", False, [("invitation", "r"), ("scopeNote", "r"), ("exampleChips", "r")]),
    "search": ("search-demo", False, [("queryField", "tl"), ("searchButton", "t"), ("listViewButton", "t"),
                                      ("resultsView", "tl"), ("resultsCount", "r"), ("loadMoreButton", "t")]),
    "playlist": ("playlist-demo", False, [("backButton", "t"), ("downloadButton", "t"), ("playAllButton", "t"), ("copyLinkButton", "t"),
                                          ("selectAllBox", "tl"), ("rangeSlider", "t"), ("filterField", "t"), ("sortCombo", "t"),
                                          ("skipBox", "t"), ("entriesList", "tl"), ("footerLabel", "r")]),
    "options": ("options-demo", True, [("videoCard", "tl"), ("audioCard", "tr"), ("qualityCombo", "r"), ("containerCombo", "r"),
                                       ("subtitlesCombo", "r"), ("embedThumbnailBox", "r"), ("changeButton", "r"), ("ownFolderBox", "r"),
                                       ("numberBox", "r"), ("previewLabel", "r"), ("downloadButton", "r")]),
    "downloads": ("downloads-demo", False, [("engineChip", "b"), ("allowanceChip", "b"), ("pauseAllButton", "t"), ("retryFailedButton", "t"),
                                            ("clearButton", "t"), ("openFolderButton", "t"), ("filterRow", "l"), ("countLabel", "r"),
                                            ("downloadsList", "tl")]),
    "items": ("playlist-items-demo-file", True, [("playlistFileBanner", "tl"), ("selectAllBox", "tl"), ("itemList", "tl"), ("playButton", "b"),
                                                 ("revealButton", "b"), ("moveUpButton", "b"), ("arrangeButton", "b"),
                                                 ("downloadMissingButton", "b"), ("saveButton", "b"), ("playAllButton", "b")]),
    "browser": ("browser:about:blank", False, [("tabStrip", "tl"), ("addressField", "t"), ("adsBadge", "t"), ("downloadThisButton", "t")]),
    "settings": ("settings:downloads", True, [("settingsNav", "tl"), ("playlistFileCheck", "r"), ("closeButton", "r")]),
}


def grab(hook, want_dialog, scratch):
    prefix = scratch / "shot"
    env = dict(os.environ)
    env.update({"QT_QPA_PLATFORM": "offscreen", "XDG_CONFIG_HOME": str(scratch / "config"),
                "XDG_DATA_HOME": str(scratch / "data"), "XDG_CACHE_HOME": str(scratch / "cache"),
                "TMPDIR": str(scratch), "PLDL_DEBUG_OPEN": hook, "PLDL_DEBUG_GRAB": f"{prefix}.png,3",
                "PLDL_DEBUG_WINDOW_SIZE": SIZE})
    for path in scratch.glob("shot*"):
        path.unlink()
    subprocess.run(["timeout", "25", str(ROOT / "scripts/dev-run.sh"), "--", "--profile", "guide"], env=env,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
    png = scratch / ("shot-dialog1.png" if want_dialog else "shot.png")
    if not png.exists():
        raise SystemExit(f"no grab for {hook}")
    return Image.open(png).convert("RGB"), json.loads((scratch / (png.name + ".json")).read_text())


def badge_pos(rect, anchor):
    x, y, w, h = rect
    return {"tl": (x, y), "t": (x + w / 2, y), "tr": (x + w, y), "l": (x, y + h / 2),
            "r": (x + w, y + h / 2), "b": (x + w / 2, y + h), "bl": (x, y + h)}[anchor]


def annotate(image, rects, callouts):
    draw = ImageDraw.Draw(image)
    fnt = ImageFont.truetype(FONT_BOLD, 17)
    n = 0
    for name, anchor in callouts:
        if name not in rects:
            print(f"  (no widget named {name}: skipped)", file=sys.stderr)
            continue
        n += 1
        x, y, w, h = rects[name]
        draw.rounded_rectangle((x - 2, y - 2, x + w + 2, y + h + 2), radius=6, outline=ACCENT, width=2)
        cx, cy = badge_pos(rects[name], anchor)
        cx = min(max(cx, 16), image.width - 16)
        cy = min(max(cy, 16), image.height - 16)
        draw.ellipse((cx - 14, cy - 14, cx + 14, cy + 14), fill=ACCENT, outline=INK, width=2)
        text = str(n)
        tw = draw.textlength(text, font=fnt)
        draw.text((cx - tw / 2, cy - 11), text, font=fnt, fill=INK)
    return image


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="pldl-guide-") as tmp:
        scratch = Path(tmp)
        for name, (hook, dialog, callouts) in PLAN.items():
            image, rects = grab(hook, dialog, scratch)
            annotate(image, rects, callouts).save(OUT / f"{name}.png", optimize=True)
            print(name, image.size)


if __name__ == "__main__":
    main()
