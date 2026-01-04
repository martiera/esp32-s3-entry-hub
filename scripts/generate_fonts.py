#!/usr/bin/env python3
"""
Generate custom LVGL fonts with extended character support (Latin Extended).
Downloads Montserrat font and generates C files for LVGL.
"""

import os
import sys
import subprocess
import urllib.request
import ssl
from pathlib import Path

# Disable SSL verification for font download
ssl._create_default_https_context = ssl._create_unverified_context

# Project paths
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
FONTS_DIR = PROJECT_ROOT / "fonts"
SRC_DIR = PROJECT_ROOT / "src"

# Font configuration
MONTSERRAT_URL = "https://github.com/JulietaUla/Montserrat/raw/master/fonts/ttf/Montserrat-Regular.ttf"
FONT_FILE = FONTS_DIR / "Montserrat-Regular.ttf"

# Font sizes to generate with extended character support
FONT_SIZES = [20]  # 20pt for person names

# Character ranges
# Basic Latin: 0x20-0x7F (space to ~)
# Latin-1 Supplement: 0xA0-0xFF (¡ to ÿ)
# Latin Extended-A: 0x100-0x17F (Ā to ſ) - includes ā, ē, ī, ō, ū, etc.
CHAR_RANGES = "0x20-0x7F,0xA0-0xFF,0x100-0x17F"

def download_font():
    """Download Montserrat font if not already present."""
    if FONT_FILE.exists():
        print(f"✓ Font already exists: {FONT_FILE}")
        return True
    
    print(f"→ Downloading Montserrat font...")
    try:
        FONTS_DIR.mkdir(parents=True, exist_ok=True)
        urllib.request.urlretrieve(MONTSERRAT_URL, FONT_FILE)
        print(f"✓ Downloaded to: {FONT_FILE}")
        return True
    except Exception as e:
        print(f"✗ Failed to download font: {e}")
        return False

def check_lv_font_conv():
    """Check if lv_font_conv is installed."""
    try:
        result = subprocess.run(["lv_font_conv", "--help"], 
                              capture_output=True, 
                              text=True)
        return result.returncode == 0
    except FileNotFoundError:
        return False

def install_lv_font_conv():
    """Install lv_font_conv via npm."""
    print("→ Installing lv_font_conv (requires Node.js/npm)...")
    try:
        result = subprocess.run(["npm", "install", "-g", "lv_font_conv"],
                              capture_output=True,
                              text=True)
        if result.returncode == 0:
            print("✓ lv_font_conv installed successfully")
            return True
        else:
            print(f"✗ Installation failed: {result.stderr}")
            return False
    except FileNotFoundError:
        print("✗ npm not found. Please install Node.js first:")
        print("  macOS: brew install node")
        print("  Linux: apt install nodejs npm")
        return False

def generate_font(size):
    """Generate LVGL font C file for specified size."""
    output_file = SRC_DIR / f"montserrat_extended_{size}.c"
    
    print(f"→ Generating {size}pt font with Latin Extended support...")
    
    cmd = [
        "lv_font_conv",
        "--font", str(FONT_FILE),
        "--size", str(size),
        "--format", "lvgl",
        "--bpp", "4",
        "--range", CHAR_RANGES,
        "--no-compress",
        "--output", str(output_file)
    ]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        
        # Add extern declaration at the top of the file
        with open(output_file, 'r') as f:
            content = f.read()
        
        # Find the font descriptor declaration and add extern
        import re
        pattern = r'(const lv_font_t montserrat_extended_\d+)'
        content = re.sub(pattern, r'extern \1', content)
        
        with open(output_file, 'w') as f:
            f.write(content)
        
        print(f"✓ Generated: {output_file}")
        return True
        
    except subprocess.CalledProcessError as e:
        print(f"✗ Font generation failed: {e.stderr}")
        return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False

def create_header_file():
    """Create header file for custom fonts."""
    header_file = SRC_DIR / "montserrat_extended.h"
    
    content = """#ifndef MONTSERRAT_EXTENDED_H
#define MONTSERRAT_EXTENDED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// Custom Montserrat fonts with Latin Extended character support
// Includes: Basic Latin, Latin-1 Supplement, and Latin Extended-A
// Character ranges: U+0020-U+007F, U+00A0-U+00FF, U+0100-U+017F
// This covers characters like: ā, ē, ī, ō, ū, ć, ś, ż, etc.

extern const lv_font_t montserrat_extended_20;

#ifdef __cplusplus
}
#endif

#endif // MONTSERRAT_EXTENDED_H
"""
    
    with open(header_file, 'w') as f:
        f.write(content)
    
    print(f"✓ Created header: {header_file}")

def main():
    print("\n=== LVGL Custom Font Generator ===\n")
    
    # Step 1: Download font
    if not download_font():
        return 1
    
    # Step 2: Check/install lv_font_conv
    if not check_lv_font_conv():
        print("! lv_font_conv not found")
        if not install_lv_font_conv():
            print("\n✗ Please install lv_font_conv manually:")
            print("  npm install -g lv_font_conv")
            return 1
    else:
        print("✓ lv_font_conv is available")
    
    # Step 3: Generate fonts
    success = True
    for size in FONT_SIZES:
        if not generate_font(size):
            success = False
    
    if not success:
        return 1
    
    # Step 4: Create header file
    create_header_file()
    
    print("\n✓ Font generation complete!")
    print("\nNext steps:")
    print("  1. Include 'montserrat_extended.h' in lvgl_ui.cpp")
    print("  2. Use &montserrat_extended_20 instead of &lv_font_montserrat_20")
    print("  3. Rebuild and upload to ESP32")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
