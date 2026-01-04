#!/usr/bin/env python3
"""
Download weather icons from Visual Crossing Weather Icons repository
https://github.com/visualcrossing/WeatherIcons
License: LGPL-3.0
"""

import os
import urllib.request
import sys

# Base URL for PNG icons (1st Set - Color)
BASE_URL = "https://raw.githubusercontent.com/visualcrossing/WeatherIcons/main/PNG/1st%20Set%20-%20Color/"

# Icons needed for Home Assistant weather conditions
ICONS = [
    "clear-day.png",       # Sunny/Clear day
    "clear-night.png",     # Clear night
    "cloudy.png",          # Cloudy/Overcast
    "partly-cloudy-day.png",   # Partly cloudy day
    "partly-cloudy-night.png", # Partly cloudy night
    "rain.png",            # Rain/Drizzle/Showers
    "snow.png",            # Snow
    "fog.png",             # Fog/Mist/Haze
    "wind.png",            # Windy
    "thunder-rain.png",    # Thunderstorm/Lightning
]

# Output directory
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "..", "pictures", "weather")

def download_icons():
    """Download weather icons from GitHub repository"""
    
    # Create output directory
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    
    print(f"Downloading weather icons to: {OUTPUT_DIR}")
    print(f"Source: {BASE_URL}")
    print("-" * 60)
    
    success_count = 0
    failed_count = 0
    
    for icon_name in ICONS:
        url = BASE_URL + icon_name
        output_path = os.path.join(OUTPUT_DIR, icon_name)
        
        try:
            print(f"Downloading {icon_name}...", end=" ")
            urllib.request.urlretrieve(url, output_path)
            size = os.path.getsize(output_path)
            print(f"✓ ({size:,} bytes)")
            success_count += 1
        except Exception as e:
            print(f"✗ Failed: {e}")
            failed_count += 1
    
    print("-" * 60)
    print(f"Downloaded: {success_count}/{len(ICONS)} icons")
    
    if failed_count > 0:
        print(f"Failed: {failed_count} icons")
        return 1
    
    print("\nWeather icons ready for conversion to LVGL format.")
    print("Use LVGL Image Converter: https://lvgl.io/tools/imageconverter")
    print("Settings: Color format=CF_TRUE_COLOR_ALPHA, Output format=C array")
    
    return 0

if __name__ == "__main__":
    sys.exit(download_icons())
