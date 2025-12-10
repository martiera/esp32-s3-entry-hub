# 📋 Build & Deployment Checklist

## Pre-Build Checklist

### ✅ Hardware Verification
- [ ] ESP32-S3-WROOM-1 board available
- [ ] INMP441 microphone module ready
- [ ] USB-C cable for programming
- [ ] Power supply adequate (5V, 1A minimum)
- [ ] (Optional) Display module prepared

### ✅ Software Installation
- [ ] VS Code installed
- [ ] PlatformIO extension installed
- [ ] Git installed
- [ ] Python 3 available (for scripts)

### ✅ Project Setup
- [ ] Repository cloned
- [ ] Project opened in VS Code
- [ ] PlatformIO recognized project
- [ ] Dependencies will download automatically

## Configuration Checklist

### ✅ Secrets Configuration
- [ ] `include/secrets.h.example` copied to `include/secrets.h`
- [ ] MQTT broker IP configured
- [ ] MQTT credentials set
- [ ] OTA password set
- [ ] (Optional) Porcupine access key added
- [ ] (Optional) Weather API key added
- [ ] (Optional) Home Assistant token added

### ✅ Hardware Configuration
- [ ] Pin assignments verified in `include/config.h`
- [ ] I2S pins match wiring (GPIO 4, 5, 6)
- [ ] (Optional) Display pins configured
- [ ] Board configuration correct in `platformio.ini`

## Build Checklist

### ✅ First Build
```bash
# In VS Code terminal or PlatformIO CLI:

# 1. Clean build (optional)
pio run --target clean

# 2. Build firmware
pio run

# Check for errors - should complete successfully
```

**Expected Output:**
```
Building .pio/build/esp32-s3-devkitc-1/firmware.bin
RAM:   [====      ]  XX.X% (used XXXXX bytes)
Flash: [====      ]  XX.X% (used XXXXX bytes)
SUCCESS
```

### ✅ Build Verification
- [ ] No compilation errors
- [ ] No linker errors
- [ ] Firmware.bin created
- [ ] Size within flash limits

## Upload Checklist

### ✅ Filesystem Upload
```bash
# Upload web files and data
pio run --target uploadfs
```

**Check:**
- [ ] Upload successful
- [ ] No errors reported
- [ ] Completes in ~30 seconds

### ✅ Firmware Upload
```bash
# Upload firmware to device
pio run --target upload
```

**Check:**
- [ ] Board connected via USB
- [ ] Correct COM port selected
- [ ] Upload completes successfully
- [ ] Device reboots automatically

### ✅ Serial Monitor
```bash
# Monitor device output
pio device monitor
```

**Expected Boot Sequence:**
```
╔═══════════════════════════════════════╗
║   ESP32-S3 Entry Hub                  ║
║   Voice-Controlled Access Panel       ║
║   Version: 1.0.0                      ║
╚═══════════════════════════════════════╝

Initializing system components...

→ Storage system... ✓
→ WiFi connection... ✓
→ MQTT client... ✓
→ Web server... ✓
→ OTA updates... ✓
→ I2S audio input... ✓
→ Porcupine wake word... ⚠️ Template mode
→ Home Assistant integration... ✓
→ Display manager... ✓

✓ System initialization complete!
```

## First Boot Checklist

### ✅ WiFi Setup
- [ ] Device creates AP: `ESP32-EntryHub-Setup`
- [ ] Connect to AP from phone/computer
- [ ] Captive portal opens (or navigate to 192.168.4.1)
- [ ] Enter WiFi credentials
- [ ] Click Save
- [ ] Device connects to WiFi
- [ ] Device gets IP address
- [ ] Note the IP address from serial monitor

### ✅ Network Verification
- [ ] Device shows "WiFi Connected" in serial
- [ ] IP address displayed
- [ ] mDNS started (entryhub.local)
- [ ] MQTT connected
- [ ] Web server started

## Web Interface Checklist

### ✅ Access Admin Panel
- [ ] Open browser
- [ ] Navigate to `http://entryhub.local` or `http://<device-ip>`
- [ ] Admin panel loads
- [ ] No JavaScript errors (check browser console)
- [ ] WebSocket connects (green status indicator)

### ✅ Test Dashboard
- [ ] System status displays
- [ ] Uptime counting
- [ ] WiFi signal showing
- [ ] Free memory displayed
- [ ] Voice status visible

### ✅ Test Navigation
- [ ] All menu items clickable
- [ ] Pages switch correctly
- [ ] No broken links
- [ ] Mobile responsive (test on phone)

## MQTT Checklist

### ✅ MQTT Connection
**Test with mosquitto_sub:**
```bash
# Subscribe to all entry hub topics
mosquitto_sub -h <broker-ip> -t "entryhub/#" -v
```

**Expected Topics:**
- [ ] `entryhub/status` - Device status published
- [ ] `entryhub/sensor/voice_command` - Voice data
- [ ] `entryhub/sensor/wifi_signal` - WiFi RSSI
- [ ] `entryhub/sensor/uptime` - Uptime value
- [ ] `entryhub/sensor/free_memory` - Memory status

### ✅ Command Execution
**Test command via MQTT:**
```bash
# Publish a test command
mosquitto_pub -h <broker-ip> -t "entryhub/command" -m "turn on the lights"
```

**Check:**
- [ ] Command received (serial monitor)
- [ ] Command processed
- [ ] Response published

## Home Assistant Checklist

### ✅ Auto-Discovery
**In Home Assistant:**
- [ ] Go to Settings > Devices & Services
- [ ] Check MQTT integration
- [ ] Look for "ESP32-EntryHub" device
- [ ] Should show auto-discovered entities

### ✅ Entities Verification
**Expected Entities:**
- [ ] `sensor.entryhub_voice_command`
- [ ] `sensor.entryhub_wifi_signal`
- [ ] `sensor.entryhub_uptime`
- [ ] `sensor.entryhub_free_memory`
- [ ] `binary_sensor.entryhub_voice_active`
- [ ] `binary_sensor.entryhub_mqtt_connected`
- [ ] `switch.entryhub_welcome_home_scene`
- [ ] `switch.entryhub_good_night_scene`
- [ ] `switch.entryhub_away_mode_scene`

### ✅ Entity Testing
- [ ] Click on entity
- [ ] History shows data
- [ ] Values updating (for sensors)
- [ ] Switches toggle (for switch entities)

## Voice System Checklist

### ✅ Audio Input
**Check Serial Monitor:**
- [ ] "Audio recording started" message
- [ ] No I2S errors
- [ ] Audio samples being read

### ✅ Porcupine Integration (if added)
- [ ] Porcupine library in `lib/porcupine/`
- [ ] Access key in `secrets.h`
- [ ] `.ppn` files in `data/keywords/`
- [ ] Initialization successful
- [ ] Wake word detection working

### ✅ Voice Testing
- [ ] Speak wake word near microphone
- [ ] Detection logged in serial monitor
- [ ] Web panel shows detection
- [ ] MQTT message published
- [ ] HA sensor updates

## OTA Update Checklist

### ✅ Web OTA
- [ ] Navigate to `http://entryhub.local/update`
- [ ] ElegantOTA page loads
- [ ] Can select firmware file
- [ ] Upload test (optional at this stage)

### ✅ Arduino OTA
```bash
# Test OTA upload (after initial USB upload)
pio run --target upload --upload-port entryhub.local
```

**Check:**
- [ ] OTA port discovered
- [ ] Upload via network works
- [ ] Device reboots after OTA

## Performance Checklist

### ✅ Stability
**Run for 10 minutes and verify:**
- [ ] No crashes or reboots
- [ ] WiFi stays connected
- [ ] MQTT stays connected
- [ ] Web panel responsive
- [ ] Memory not decreasing (memory leak check)
- [ ] CPU not overheating

### ✅ Response Times
- [ ] Web page loads < 2 seconds
- [ ] API responses < 200ms
- [ ] WebSocket updates instant
- [ ] Voice detection < 500ms (with Porcupine)

## Troubleshooting Checklist

### ❌ If Build Fails
- [ ] Check PlatformIO installation
- [ ] Update platform: `pio platform update espressif32`
- [ ] Clean build: `pio run --target clean`
- [ ] Check for missing libraries
- [ ] Verify `secrets.h` exists

### ❌ If Upload Fails
- [ ] Check USB cable connection
- [ ] Verify correct COM port
- [ ] Try different USB port
- [ ] Press BOOT button during upload
- [ ] Check drivers installed

### ❌ If WiFi Fails
- [ ] Verify WiFi credentials
- [ ] Check 2.4GHz network (ESP32 doesn't support 5GHz)
- [ ] Move closer to router
- [ ] Reset WiFi settings
- [ ] Check serial for error messages

### ❌ If MQTT Fails
- [ ] Verify broker IP and port
- [ ] Check broker is running
- [ ] Test with mosquitto_pub/sub
- [ ] Verify credentials
- [ ] Check firewall settings

### ❌ If Web Panel Fails
- [ ] Verify filesystem uploaded
- [ ] Clear browser cache
- [ ] Check browser console for errors
- [ ] Try different browser
- [ ] Check device IP is correct

## Production Deployment Checklist

### ✅ Final Checks Before Deployment
- [ ] All features tested
- [ ] Stable for 24+ hours
- [ ] Backup configuration exported
- [ ] Documentation reviewed
- [ ] Enclosure prepared (if using)
- [ ] Power supply stable
- [ ] Network connection reliable

### ✅ Installation
- [ ] Mount in desired location
- [ ] Connect power
- [ ] Verify network connectivity
- [ ] Test from installation location
- [ ] Configure final settings
- [ ] Test voice from typical distance
- [ ] Train family members on usage

### ✅ Maintenance Setup
- [ ] Document device IP address
- [ ] Save configuration backup
- [ ] Set up monitoring in HA
- [ ] Create restart automation (optional)
- [ ] Schedule periodic checks

## Success Criteria

### ✅ System is Ready When:
- [x] Builds without errors
- [x] Uploads successfully
- [x] Boots and initializes
- [x] Connects to WiFi
- [x] MQTT connected
- [x] Web panel accessible
- [x] Home Assistant entities discovered
- [x] Voice system running (even in template mode)
- [x] OTA working
- [x] Stable operation

## 🎉 Deployment Complete!

If all checkboxes above are checked, your ESP32-S3 Entry Hub is:
- ✅ Built successfully
- ✅ Uploaded and running
- ✅ Connected to network
- ✅ Integrated with Home Assistant
- ✅ Ready for use and customization

**Next Steps:**
1. Add Porcupine library for wake words (see SETUP.md)
2. Create custom voice commands
3. Set up automations in Home Assistant
4. Customize the web interface
5. Add display hardware (optional)

**Enjoy your smart entry hub!** 🏠✨
