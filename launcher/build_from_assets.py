"""
E-OS Launcher V6.0 - Build icons_data.h from assets/ folder
============================================================
Put your PNG sprites in launcher/assets/ with these names:
  doom.png, snake.png, flappy.png, space.png, arkanoid.png,
  pacman.png, mode7.png, wire3d.png, strike.png,
  platform.png, tetris.png, dungeon.png, towerdefense.png,
  settings.png, flight.png

Transparent PNGs -> auto-filled with dark card BG (12,12,20)
Missing files   -> red placeholder square

Output: icons_data.h + PNG previews (preview/ folder)
Usage:  python build_from_assets.py
"""

from PIL import Image
import os

# ══════════════════════════════════════════════════════════════
#  ESP32 BGR565 Byte-Swap (Little-Endian fix)
# ══════════════════════════════════════════════════════════════

def rgb565_swapped(r, g, b):
    """RGB -> BGR565 with byte swap for ESP32 TFT endianness"""
    color = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3)
    return ((color & 0xFF) << 8) | (color >> 8)

# ══════════════════════════════════════════════════════════════
#  Game list: (folder_name, C_array_name_prefix, display_name)
# ══════════════════════════════════════════════════════════════

GAMES = [
    ("doom",     "iconDoom",     "Doom"),
    ("snake",    "iconSnake",    "Snake"),
    ("flappy",   "iconFlappy",   "Flappy Bird"),
    ("space",    "iconSpace",    "Space Invaders"),
    ("arkanoid", "iconArkanoid", "Arkanoid"),
    ("pacman",   "iconPacman",   "Pac-Man"),
    ("mode7",    "iconMode7",    "Mode 7"),
    ("wire3d",   "iconWire3D",   "3D Wireframe"),
    ("strike",   "iconStrike",   "Strike"),
    ("platform", "iconPlatform", "Platformer"),
    ("tetris",   "iconTetris",   "Tetris"),
    ("dungeon",  "iconDungeon",  "Dungeon"),
    ("towerdefense", "iconTowerDefense", "Tower Defense"),
    ("settings", "iconSettings", "Settings"),
    ("flight",   "iconFlight",   "Flight"),
]

CARD_BG = (12, 12, 20)   # Dark launcher card background
PLACEHOLDER_COLOR = (255, 30, 30)  # Red for missing images


def process_image(filepath, size):
    """
    Open PNG, handle transparency, resize to (size x size).
    Returns PIL Image in RGB mode.
    """
    img = Image.open(filepath).convert('RGBA')

    # If image has transparency, composite onto dark card background
    if img.mode == 'RGBA':
        bg = Image.new('RGB', img.size, CARD_BG)
        bg.paste(img, mask=img.split()[3])  # Use alpha channel as mask
        img = bg
    else:
        img = img.convert('RGB')

    # Resize with high-quality LANCZOS
    img = img.resize((size, size), Image.LANCZOS)
    return img


def create_placeholder(size):
    """Red square for missing images."""
    img = Image.new('RGB', (size, size), PLACEHOLDER_COLOR)
    return img


def image_to_rgb565_array(img, name):
    """PIL Image -> C-array RGB565 string (BGR swapped)"""
    pixels = img.load()
    w, h = img.size
    total = w * h
    lines = [f"const uint16_t {name}[{total}] PROGMEM = {{"]
    for y in range(h):
        row_vals = []
        for x in range(w):
            r, g, b = pixels[x, y]
            val = rgb565_swapped(r, g, b)
            row_vals.append(f"0x{val:04X}")
        lines.append("    " + ", ".join(row_vals) + ",")
    lines.append("};")
    return "\n".join(lines)


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    assets_dir = os.path.join(script_dir, "assets")
    preview_dir = os.path.join(script_dir, "preview")
    os.makedirs(preview_dir, exist_ok=True)

    big_arrays   = []
    small_arrays = []
    big_imgs     = []
    small_imgs   = []
    found_count  = 0

    print("=" * 60)
    print("  E-OS Launcher V6.0 - Asset Builder")
    print("=" * 60)
    print()

    for folder_name, array_prefix, display_name in GAMES:
        png_path = os.path.join(assets_dir, f"{folder_name}.png")
        big_name   = f"{array_prefix}Big"
        small_name = f"{array_prefix}Small"

        if os.path.exists(png_path):
            print(f"  [FOUND]  {display_name:16s} -> {folder_name}.png")
            img_big   = process_image(png_path, 64)
            img_small = process_image(png_path, 32)
            found_count += 1
        else:
            print(f"  [MISSING] {display_name:16s} -> red placeholder")
            img_big   = create_placeholder(64)
            img_small = create_placeholder(32)

        big_imgs.append(img_big)
        small_imgs.append(img_small)

        # Save preview PNGs
        img_big.save(os.path.join(preview_dir, f"{big_name}.png"))
        img_small.save(os.path.join(preview_dir, f"{small_name}.png"))

        # Generate C-arrays
        big_arrays.append(f"// {display_name} Big Icon (64x64)\n" +
                          image_to_rgb565_array(img_big, big_name))
        small_arrays.append(f"// {display_name} Small Icon (32x32)\n" +
                            image_to_rgb565_array(img_small, small_name))

    # ══════════════════════════════════════════════════════════
    #  WRITE icons_data.h
    # ══════════════════════════════════════════════════════════
    header_path = os.path.join(script_dir, "icons_data.h")
    with open(header_path, "w", encoding="utf-8") as f:
        f.write("#ifndef ICONS_DATA_H\n")
        f.write("#define ICONS_DATA_H\n\n")
        f.write("#include <pgmspace.h>\n\n")
        f.write("// ══════════════════════════════════════════════════════════════\n")
        f.write("//  E-OS Launcher V6.0 - PROGMEM Icon Database\n")
        f.write("//  Built from assets/ folder via build_from_assets.py\n")
        f.write(f"//  {found_count}/13 assets found, others are placeholders\n")
        f.write("// ══════════════════════════════════════════════════════════════\n\n")
        for i in range(len(GAMES)):
            f.write(big_arrays[i])
            f.write("\n\n")
            f.write(small_arrays[i])
            f.write("\n\n")
        f.write("#endif\n")

    # ══════════════════════════════════════════════════════════════
    #  CONTACT SHEETS (all icons side by side)
    # ══════════════════════════════════════════════════════════════
    contact_big = Image.new("RGB", (64 * len(GAMES), 64))
    for i, img in enumerate(big_imgs):
        contact_big.paste(img, (i * 64, 0))
    contact_big.save(os.path.join(preview_dir, "_all_big.png"))

    contact_small = Image.new("RGB", (32 * len(GAMES), 32))
    for i, img in enumerate(small_imgs):
        contact_small.paste(img, (i * 32, 0))
    contact_small.save(os.path.join(preview_dir, "_all_small.png"))

    # ══════════════════════════════════════════════════════════════
    #  SUMMARY
    # ══════════════════════════════════════════════════════════════
    total_bytes = len(GAMES) * 64 * 64 * 2 + len(GAMES) * 32 * 32 * 2
    print()
    print(f"  {found_count}/13 assets found")
    print(f"  icons_data.h  -> {header_path}")
    print(f"  PNG previews  -> {preview_dir}/")
    print(f"  Flash usage   -> {total_bytes} bytes ({total_bytes / 1024:.1f} KB)")
    print()
    if found_count < 13:
        print("  TIP: Add missing PNG files to assets/, then run again!")
        print("       Names: snake.png flappy.png space.png arkanoid.png")
        print("              pacman.png mode7.png wire3d.png strike.png")
        print("              platform.png settings.png flight.png")
    print("=" * 60)


if __name__ == "__main__":
    main()
