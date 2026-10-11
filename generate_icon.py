import math
from PIL import Image, ImageDraw, ImageFilter

def create_paint3d_icon(size):
    # Higher resolution canvas for smooth anti-aliased rendering
    scale = 4
    canvas_size = size * scale
    img = Image.new('RGBA', (canvas_size, canvas_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Draw rounded square base / background droplet
    margin = canvas_size * 0.08
    r = canvas_size * 0.2

    # Paint 3D colorful ribbon loop / droplet shape
    # Vibrant modern gradients (Teal, Blue, Purple, Magenta)
    width = canvas_size
    height = canvas_size

    # Draw 3D teardrop/ribbon layers
    # Layer 1: Purple-Blue 3D sphere/droplet back
    for rad in range(int(height * 0.38), 0, -1):
        t = rad / (height * 0.38)
        # Gradient color from deep purple to turquoise blue
        r_c = int(100 * (1-t) + 0 * t)
        g_c = int(50 * (1-t) + 200 * t)
        b_c = int(240 * (1-t) + 255 * t)
        alpha = int(255 * min(1.0, (1 - t*0.2)))
        cx = width * 0.52 - (1-t)*width*0.05
        cy = height * 0.52 - (1-t)*height*0.05
        draw.ellipse([cx - rad, cy - rad, cx + rad, cy + rad], fill=(r_c, g_c, b_c, alpha))

    # Layer 2: Main Paint 3D Ribbon dynamic ribbon path
    points = []
    num_pts = 100
    for i in range(num_pts):
        t = i / float(num_pts)
        angle = t * 2 * math.pi
        # Ribbon loop formula
        x = width * (0.5 + 0.32 * math.sin(angle) * (0.8 + 0.2 * math.cos(angle)))
        y = height * (0.5 - 0.32 * math.cos(angle) + 0.1 * math.sin(2*angle))
        points.append((x, y))

    # Draw colorful overlapping brush ribbon strokes
    for i in range(len(points) - 1):
        p1 = points[i]
        p2 = points[i+1]
        t = i / len(points)

        # Paint 3D signature color wheel flow: Cyan -> Blue -> Purple -> Pink -> Orange -> Yellow
        if t < 0.25:
            # Cyan to Blue
            sub_t = t / 0.25
            r_c = int(0 * (1-sub_t) + 30 * sub_t)
            g_c = int(210 * (1-sub_t) + 120 * sub_t)
            b_c = int(255 * (1-sub_t) + 255 * sub_t)
        elif t < 0.5:
            # Blue to Magenta
            sub_t = (t - 0.25) / 0.25
            r_c = int(30 * (1-sub_t) + 230 * sub_t)
            g_c = int(120 * (1-sub_t) + 40 * sub_t)
            b_c = int(255 * (1-sub_t) + 210 * sub_t)
        elif t < 0.75:
            # Magenta to Orange
            sub_t = (t - 0.5) / 0.25
            r_c = int(230 * (1-sub_t) + 255 * sub_t)
            g_c = int(40 * (1-sub_t) + 140 * sub_t)
            b_c = int(210 * (1-sub_t) + 30 * sub_t)
        else:
            # Orange to Cyan
            sub_t = (t - 0.75) / 0.25
            r_c = int(255 * (1-sub_t) + 0 * sub_t)
            g_c = int(140 * (1-sub_t) + 210 * sub_t)
            b_c = int(30 * (1-sub_t) + 255 * sub_t)

        stroke_w = int(canvas_size * 0.16)
        draw.line([p1, p2], fill=(r_c, g_c, b_c, 240), width=stroke_w)
        # Add smooth end caps
        draw.ellipse([p1[0]-stroke_w/2, p1[1]-stroke_w/2, p1[0]+stroke_w/2, p1[1]+stroke_w/2], fill=(r_c, g_c, b_c, 240))

    # Add 3D glossy highlight / specular reflection
    highlight = Image.new('RGBA', (canvas_size, canvas_size), (0,0,0,0))
    hl_draw = ImageDraw.Draw(highlight)
    hl_draw.ellipse([width * 0.25, height * 0.20, width * 0.55, height * 0.45], fill=(255, 255, 255, 120))
    highlight = highlight.filter(ImageFilter.GaussianBlur(radius=canvas_size*0.05))

    img = Image.alpha_composite(img, highlight)

    # Resize down to target size with high quality antialiasing
    img = img.resize((size, size), Image.Resampling.LANCZOS)
    return img

# Create multi-size icon sizes: 256, 128, 96, 64, 48, 32, 24, 16
sizes = [256, 128, 96, 64, 48, 32, 24, 16]
images = [create_paint3d_icon(s) for s in sizes]

# Save primary PNG and ICO
images[0].save('app.png')
images[0].save('app.ico', format='ICO', sizes=[(s, s) for s in sizes], append_images=images[1:])
print("Successfully generated app.ico and app.png")
