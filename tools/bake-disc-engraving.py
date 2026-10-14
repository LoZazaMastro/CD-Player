"""Bake the existing Doto typeface into a relief mask for the native disc shader."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
root = Path(__file__).resolve().parents[1]
font = ImageFont.truetype(str(root / 'third_party/fonts/Doto-SemiBold.ttf'), 110)
box = font.getbbox('CD Player')
mask = Image.new('L', (1024, 256))
scale = min(984 / (box[2]-box[0]), 216 / (box[3]-box[1]))
font = ImageFont.truetype(str(root / 'third_party/fonts/Doto-SemiBold.ttf'), int(110*scale))
box = font.getbbox('CD Player')
ImageDraw.Draw(mask).text(((1024-(box[2]-box[0]))/2-box[0], (256-(box[3]-box[1]))/2-box[1]), 'CD Player', font=font, fill=255)
out = root / 'assets/disc/engraving.png'
out.parent.mkdir(exist_ok=True)
mask.save(out)
