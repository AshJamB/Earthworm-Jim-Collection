"""Builds every app icon from one source image.

Usage:  python tools/make_icons.py path\to\icon.png [--zoom 1.15] [--dry-run]

--zoom crops in on the centre (1.0 = whole image) so the subject fills more of a tiny icon.

The source should be a square PNG, ideally 1024x1024 (anything square of 256 px or more works; it is resized
with high-quality filtering). It writes:
  res/app.ico                               Windows exe icon (16, 24, 32, 48, 64, 128, 256 px)
  res/app_icon_preview.png                  256 px preview
  android/app/src/main/res/mipmap-*/ic_launcher.png   Android launcher icons (48 / 72 / 96 / 144 / 192 px)
Needs Pillow (pip install pillow).
"""
import os
import sys
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ANDROID = {'mdpi': 48, 'hdpi': 72, 'xhdpi': 96, 'xxhdpi': 144, 'xxxhdpi': 192}
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]


def main():
    argv = sys.argv[1:]
    zoom = 1.0
    if '--zoom' in argv:
        i = argv.index('--zoom')
        zoom = float(argv[i + 1])
        del argv[i:i + 2]
    args = [a for a in argv if not a.startswith('--')]
    dry = '--dry-run' in argv
    if len(args) != 1:
        print(__doc__)
        return 1
    src = Image.open(args[0]).convert('RGBA')
    if src.width != src.height or src.width < 256:
        print('The image must be square and at least 256x256 (it is %dx%d).' % src.size)
        return 1

    if zoom > 1.0:
        side = int(src.width / zoom)
        off = (src.width - side) // 2
        src = src.crop((off, off, off + side, off + side))

    def sized(n):
        return src.resize((n, n), Image.LANCZOS)

    outputs = []
    outputs.append((os.path.join(ROOT, 'res', 'app.ico'), 'ico'))
    outputs.append((os.path.join(ROOT, 'res', 'app_icon_preview.png'), 256))
    for d, px in ANDROID.items():
        outputs.append((os.path.join(ROOT, 'android', 'app', 'src', 'main', 'res', 'mipmap-' + d, 'ic_launcher.png'), px))
    for path, kind in outputs:
        print(('would write ' if dry else 'writing ') + os.path.relpath(path, ROOT))
        if dry:
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        if kind == 'ico':
            src.save(path, format='ICO', sizes=[(s, s) for s in ICO_SIZES])
        else:
            sized(kind).save(path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
