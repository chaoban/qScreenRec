"""Generate qScreenRec icons and MSIX assets. Run: python scripts/make_icons.py"""
from PIL import Image, ImageDraw, ImageFont
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICON_DIR = os.path.join(ROOT, "resources", "icons")
ASSET_DIR = os.path.join(ROOT, "packaging", "msix", "Assets")
S = 1024  # master size


def gradient_square(size, top=(249, 166, 64), bottom=(232, 94, 28), radius_ratio=0.22):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    grad = Image.new("RGBA", (size, size))
    gd = ImageDraw.Draw(grad)
    for y in range(size):
        t = y / (size - 1)
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,)
        gd.line([(0, y), (size, y)], fill=c)
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, size - 1, size - 1], radius=int(size * radius_ratio), fill=255)
    img.paste(grad, (0, 0), mask)
    return img


def app_icon(size=S):
    img = gradient_square(size)
    d = ImageDraw.Draw(img)
    # monitor outline
    m = size * 0.20
    top, bottom = size * 0.24, size * 0.68
    w = size * 0.055
    d.rounded_rectangle([m, top, size - m, bottom], radius=int(size * 0.06), outline=(255, 255, 255, 255), width=int(w))
    # stand
    d.rounded_rectangle([size * 0.42, bottom, size * 0.58, size * 0.76], radius=0, fill=(255, 255, 255, 255))
    d.rounded_rectangle([size * 0.32, size * 0.75, size * 0.68, size * 0.80], radius=int(size * 0.025), fill=(255, 255, 255, 255))
    # record dot
    cx, cy, r = size * 0.5, (top + bottom) / 2, size * 0.11
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(255, 255, 255, 255))
    r2 = r * 0.62
    d.ellipse([cx - r2, cy - r2, cx + r2, cy + r2], fill=(226, 43, 43, 255))
    return img


def rec_icon(size=S):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    r = size * 0.46
    c = size / 2
    d.ellipse([c - r, c - r, c + r, c + r], fill=(255, 255, 255, 255))
    r2 = r * 0.80
    d.ellipse([c - r2, c - r2, c + r2, c + r2], fill=(226, 43, 43, 255))
    return img


def save_png(img, path, size):
    img.resize((size, size), Image.LANCZOS).save(path)


def main():
    os.makedirs(ICON_DIR, exist_ok=True)
    os.makedirs(ASSET_DIR, exist_ok=True)
    icon = app_icon()
    rec = rec_icon()
    save_png(icon, os.path.join(ICON_DIR, "app.png"), 256)
    save_png(rec, os.path.join(ICON_DIR, "recording.png"), 64)
    icon.save(os.path.join(ICON_DIR, "app.ico"), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])

    # MSIX visual assets (scale-100 + scale-200)
    squares = {"Square44x44Logo": 44, "Square150x150Logo": 150, "StoreLogo": 50, "Square71x71Logo": 71}
    for name, px in squares.items():
        for scale in (100, 200):
            save_png(icon, os.path.join(ASSET_DIR, f"{name}.scale-{scale}.png"), px * scale // 100)
    for px in (16, 24, 32, 48, 256):
        save_png(icon, os.path.join(ASSET_DIR, f"Square44x44Logo.targetsize-{px}_altform-unplated.png"), px)

    def wide(w, h, path):
        canvas = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        side = int(h * 0.8)
        canvas.paste(icon.resize((side, side), Image.LANCZOS), ((w - side) // 2, (h - side) // 2), icon.resize((side, side), Image.LANCZOS))
        canvas.save(path)
    for scale in (100, 200):
        wide(310 * scale // 100, 150 * scale // 100, os.path.join(ASSET_DIR, f"Wide310x150Logo.scale-{scale}.png"))
        wide(620 * scale // 100, 300 * scale // 100, os.path.join(ASSET_DIR, f"SplashScreen.scale-{scale}.png"))
    print("icons generated")


if __name__ == "__main__":
    main()
