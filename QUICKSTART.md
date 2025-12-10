# 🚀 Quick Start Guide

Get your ESP32-S3 Entry Hub running in 5 minutes!

## Prerequisites

- ✅ ESP32-S3-WROOM-1 development board
- ✅ INMP441 I2S microphone
- ✅ USB-C cable
- ✅ Computer with VS Code
- ✅ WiFi network

## Step 1: Install Software (2 minutes)

### Install VS Code and PlatformIO
```bash
# 1. Download VS Code
# https://code.visualstudio.com/

# 2. Open VS Code
# 3. Go to Extensions (Ctrl+Shift+X)
# 4. Search "PlatformIO IDE"
# 5. Click Install
# 6. Restart VS Code
```

## Step 2: Hardware Wiring (1 minute)

### Connect INMP441 to ESP32-S3
```
INMP441 Pin  →  ESP32-S3 Pin
─────────────────────────────
SCK          →  GPIO 4
WS           →  GPIO 5  
SD           →  GPIO 6
L/R          →  GND
VDD          →  3.3V
GND          →  GND
```

## Step 3: Setup Project (1 minute)

### Clone and Configure
```bash
# Clone project
cd ~/WS/github.com/martiera
git add esp32-s3-entry-hub
cd esp32-s3-entry-hub

# Open in VS Code
code .

# Create secrets file
cp include/secrets.h.example include/secrets.h

# Edit secrets.h with your info (optional - can configure via WiFi portal)
```

### Minimal Configuration
Edit `include/secrets.h`:
```cpp
// Just set these (others optional):
#define MQTT_BROKER "192.168.1.100"  // Your MQTT broker IP
#define OTA_PASSWORD "admin123"       // For firmware updates
```

## Step 4: Build and Upload (1 minute)

### Using PlatformIO GUI
1. Click PlatformIO icon in VS Code sidebar
2. Click "Upload Filesystem Image" (uploads web files)
3. Click "Upload and Monitor" (uploads firmware and shows logs)

### Using Command Line
```bash
# Upload web files
pio run --target uploadfs

# Build and upload firmware
pio run --target upload

# Monitor serial output
pio device monitor
```

## Step 5: First Boot Setup (30 seconds)

### WiFi Configuration
1. Device creates WiFi: `ESP32-EntryHub-Setup`
2. Connect to this network from your phone
3. Portal opens automatically (or go to 192.168.4.1)
4. Enter your WiFi credentials
5. Click Save

Device connects to your WiFi and restarts.

## Step 6: Access Admin Panel (10 seconds)

### Open Web Interface
```
http://entryhub.local
```

Or check serial monitor for IP address:
```
http://192.168.1.xxx
```

## 🎉 You're Done!

### What's Working Now
- ✅ Voice wake word detection (simulated - add Porcupine library)
- ✅ Web admin panel
- ✅ MQTT connectivity
- ✅ Home Assistant auto-discovery
- ✅ OTA updates at http://entryhub.local/update
- ✅ Real-time WebSocket updates

### Test It Out
1. **Dashboard**: View system status
2. **Voice Control**: Configure wake word
3. **Scenes**: Trigger quick actions
4. **Home Assistant**: Check auto-discovered entities

## Next Steps

### Add Porcupine Wake Word
```bash
# 1. Sign up at https://picovoice.ai/
# 2. Get access key
# 3. Download Porcupine Arduino library
# 4. Place in lib/porcupine/
# 5. Add access key to secrets.h
# 6. Rebuild and upload
```

### Add TensorFlow Lite Models
```bash
# 1. Train or download .tflite models
# 2. Place in data/models/
# 3. Update code to load models
# 4. Rebuild
```

### Add Display (Optional)
See `SETUP.md` for LVGL display integration guide.

## Troubleshooting

### Can't Connect to WiFi Portal
- Power cycle the device
- Check "ESP32-EntryHub-Setup" network exists
- Try connecting manually to 192.168.4.1

### Web Panel Not Loading
- Check device IP in serial monitor
- Try clearing browser cache
- Verify filesystem uploaded: `pio run --target uploadfs`

### MQTT Not Connecting
- Check broker IP in secrets.h
- Verify broker is running
- Check credentials

### Voice Not Working
- This is normal! Porcupine library needs to be added
- Check serial monitor - you'll see "template mode" message
- Follow Porcupine integration steps in SETUP.md

## Getting Help

- 📖 Full documentation: See SETUP.md
- 🐛 Issues: Open GitHub issue
- 💬 Questions: GitHub Discussions
- 📚 API docs: See README.md

## Enjoy Your Smart Entry Hub! 🏠✨
