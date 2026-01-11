#!/usr/bin/env python3
"""
Convert gate JPEG icons to LVGL C arrays with transparent white background
Requires: pillow (pip install pillow)
"""

import os
from PIL import Image

# Icon size for display (96x80 pixels to fill gate card)
ICON_SIZE = (96, 80)

# Input and output directories
INPUT_DIR = os.path.join(os.path.dirname(__file__), "..", "pictures")
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "..", "src", "gate_icons")

def rgb888_to_rgb565(r, g, b):
    """Convert RGB888 to RGB565"""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

def is_white(r, g, b, threshold=240):
    """Check if pixel is close to white"""
    return r >= threshold and g >= threshold and b >= threshold

def convert_gate_icon(input_path, output_name):
    """Convert JPEG gate icon to LVGL C array with transparent white background"""
    
    # Open and resize image
    img = Image.open(input_path)
    img = img.convert("RGB")
    img = img.resize(ICON_SIZE, Image.Resampling.LANCZOS)
    
    # Get pixel data
    pixels = img.load()
    width, height = img.size
    
    # Generate C++ file (required for PROGMEM)
    icon_var = output_name.replace("-", "_")
    output_file = os.path.join(OUTPUT_DIR, f"{icon_var}.cpp")
    
    with open(output_file, 'w') as f:
        f.write('#include "lvgl.h"\n')
        f.write('#include <Arduino.h>\n\n')
        f.write(f'// Gate icon: {output_name} ({width}x{height} RGB565 with alpha)\n')
        
        # Write interleaved data (RGB565 + Alpha)
        # LVGL LV_IMG_CF_TRUE_COLOR_ALPHA expects:
        # Byte 0: Low 8 bits of RGB565
        # Byte 1: High 8 bits of RGB565
        # Byte 2: Alpha (0-255)
        
        f.write(f'const uint8_t {icon_var}_data[] PROGMEM = {{\n')
        
        for y in range(height):
            f.write('    ')
            for x in range(width):
                r, g, b = pixels[x, y]
                
                # Make white pixels transparent
                if is_white(r, g, b):
                    a = 0  # Fully transparent
                else:
                    a = 255  # Fully opaque
                
                # Convert to RGB565
                rgb565 = rgb888_to_rgb565(r, g, b)
                
                # Write bytes: Low, High, Alpha
                low_byte = rgb565 & 0xFF
                high_byte = (rgb565 >> 8) & 0xFF
                
                f.write(f'0x{low_byte:02X}, 0x{high_byte:02X}, 0x{a:02X}, ')
            f.write('\n')
        
        f.write('};\n\n')
        
        # Write LVGL image descriptor with extern for C++ linkage
        f.write(f'extern const lv_img_dsc_t {icon_var} = {{\n')
        f.write('    .header = {\n')
        f.write('        .cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n')
        f.write('        .always_zero = 0,\n')
        f.write('        .reserved = 0,\n')
        f.write(f'        .w = {width},\n')
        f.write(f'        .h = {height},\n')
        f.write('    },\n')
        f.write(f'    .data_size = {width * height * 3},\n')
        f.write(f'    .data = {icon_var}_data,\n')
        f.write('};\n')
    
    return output_file

def create_header():
    """Create header file with gate icon declarations"""
    header_file = os.path.join(OUTPUT_DIR, "gate_icons.h")
    
    with open(header_file, 'w') as f:
        f.write('#ifndef GATE_ICONS_H\n')
        f.write('#define GATE_ICONS_H\n\n')
        f.write('#include "lvgl.h"\n\n')
        f.write('// Gate icon declarations\n')
        f.write('extern const lv_img_dsc_t gate_open;\n')
        f.write('extern const lv_img_dsc_t gate_close;\n\n')
        f.write('#endif // GATE_ICONS_H\n')
    
    print(f"✓ Created header: {header_file}")

def main():
    # Create output directory
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    
    print("Converting gate icons to LVGL format...")
    print(f"Icon size: {ICON_SIZE[0]}x{ICON_SIZE[1]}")
    print()
    
    # Convert gate icons
    icons = [
        ("gate-open.jpg", "gate_open"),
        ("gate-close.jpg", "gate_close"),
    ]
    
    for filename, output_name in icons:
        input_path = os.path.join(INPUT_DIR, filename)
        if os.path.exists(input_path):
            output_file = convert_gate_icon(input_path, output_name)
            size_kb = os.path.getsize(output_file) / 1024
            print(f"✓ {filename:20} -> {output_name:20} ({size_kb:.1f} KB)")
        else:
            print(f"✗ {filename:20} -> NOT FOUND")
    
    print()
    
    # Create header file
    create_header()
    
    print("\nDone! Gate icons converted successfully.")
    print("\nTo use in your code:")
    print('  #include "gate_icons/gate_icons.h"')
    print('  lv_img_set_src(gateIcon, &gate_open);')
    print('  lv_img_set_src(gateIcon, &gate_close);')

if __name__ == "__main__":
    main()
