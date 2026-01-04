#!/usr/bin/env python3
"""
Convert PNG weather icons to LVGL C arrays
Requires: pillow (pip install pillow)
"""

import os
import sys
from PIL import Image

# Icon size for display (96x96 pixels for larger display)
ICON_SIZE = (96, 96)

# Input and output directories
INPUT_DIR = os.path.join(os.path.dirname(__file__), "..", "pictures", "weather")
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "..", "src", "weather_icons")

def rgb888_to_rgb565(r, g, b):
    """Convert RGB888 to RGB565"""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

def convert_icon(input_path, output_name):
    """Convert PNG icon to LVGL C array with alpha channel"""
    
    # Open and resize image
    img = Image.open(input_path)
    img = img.convert("RGBA")  # Ensure RGBA mode
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
        f.write(f'// Weather icon: {output_name} ({width}x{height} RGB565 with alpha)\n')
        
        # Write interleaved data (RGB565 + Alpha)
        # LVGL LV_IMG_CF_TRUE_COLOR_ALPHA expects:
        # Byte 0: Low 8 bits of RGB565
        # Byte 1: High 8 bits of RGB565
        # Byte 2: Alpha (0-255)
        
        f.write(f'const uint8_t {icon_var}_data[] PROGMEM = {{\n')
        
        for y in range(height):
            f.write('    ')
            for x in range(width):
                r, g, b, a = pixels[x, y]
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
    """Create header file with all icon declarations"""
    header_file = os.path.join(OUTPUT_DIR, "weather_icons.h")
    
    icons = [
        "clear_day",
        "clear_night",
        "cloudy",
        "partly_cloudy_day",
        "partly_cloudy_night",
        "rain",
        "snow",
        "fog",
        "wind",
        "thunder_rain"
    ]
    
    with open(header_file, 'w') as f:
        f.write('#ifndef WEATHER_ICONS_H\n')
        f.write('#define WEATHER_ICONS_H\n\n')
        f.write('#include "lvgl.h"\n\n')
        
        for icon in icons:
            f.write(f'extern const lv_img_dsc_t {icon};\n')
        
        f.write('\n#endif // WEATHER_ICONS_H\n')
    
    return header_file

def main():
    # Check for PIL
    try:
        from PIL import Image
    except ImportError:
        print("Error: Pillow not installed. Install with: pip install pillow")
        return 1
    
    # Create output directory
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    
    print(f"Converting weather icons...")
    print(f"Input: {INPUT_DIR}")
    print(f"Output: {OUTPUT_DIR}")
    print(f"Size: {ICON_SIZE[0]}x{ICON_SIZE[1]} pixels")
    print("-" * 60)
    
    icons = {
        "clear-day.png": "clear_day",
        "clear-night.png": "clear_night",
        "cloudy.png": "cloudy",
        "partly-cloudy-day.png": "partly_cloudy_day",
        "partly-cloudy-night.png": "partly_cloudy_night",
        "rain.png": "rain",
        "snow.png": "snow",
        "fog.png": "fog",
        "wind.png": "wind",
        "thunder-rain.png": "thunder_rain",
    }
    
    converted = []
    for input_name, output_name in icons.items():
        input_path = os.path.join(INPUT_DIR, input_name)
        
        if not os.path.exists(input_path):
            print(f"⚠ Skipping {input_name} (not found)")
            continue
        
        try:
            output_file = convert_icon(input_path, output_name)
            size = os.path.getsize(output_file)
            print(f"✓ {output_name:20} -> {os.path.basename(output_file):25} ({size:,} bytes)")
            converted.append(output_name)
        except Exception as e:
            print(f"✗ {output_name:20} Failed: {e}")
    
    if converted:
        header_file = create_header()
        print("-" * 60)
        print(f"✓ Created header: {os.path.basename(header_file)}")
        print(f"\nConverted {len(converted)}/{len(icons)} icons")
        print("\nTo use in code:")
        print('  #include "weather_icons/weather_icons.h"')
        print('  lv_img_set_src(img, &clear_day);')
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
