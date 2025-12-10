# ESP32-S3 Entry Hub - Setup Guide

## Quick Start

### 1. Hardware Setup

#### Required Components
- ESP32-S3-WROOM-1 development board (16MB Flash recommended)
- INMP441 I2S Digital Microphone
- USB-C cable for programming and power
- (Optional) IPS touchscreen display

#### Wiring - INMP441 Microphone
```
INMP441          ESP32-S3
───────          ────────
SCK      ──────  GPIO 4
WS       ──────  GPIO 5
SD       ──────  GPIO 6
L/R      ──────  GND (for left channel)
VDD      ──────  3.3V
GND      ──────  GND
```

#### Future Display Wiring (IPS + Touch)
```
Display SPI      ESP32-S3
───────────      ────────
MOSI     ──────  GPIO 11
MISO     ──────  GPIO 13
SCK      ──────  GPIO 12
CS       ──────  GPIO 10
DC       ──────  GPIO 9
RST      ──────  GPIO 8
BL       ──────  GPIO 7

Touch I2C        ESP32-S3
─────────        ────────
SDA      ──────  GPIO 17
SCL      ──────  GPIO 18
```

### 2. Software Setup

#### Install PlatformIO
1. Install [Visual Studio Code](https://code.visualstudio.com/)
2. Install PlatformIO IDE extension from VS Code marketplace
3. Restart VS Code

#### Clone Project
```bash
git clone <your-repo-url>
cd esp32-s3-entry-hub
code .
```

### 3. Configure Credentials

Create `include/secrets.h` from the template:
```bash
cp include/secrets.h.example include/secrets.h
```

Edit `include/secrets.h` with your credentials:
```cpp
// WiFi (optional - can configure via portal)
#define WIFI_SSID "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"

// MQTT Broker
#define MQTT_BROKER "192.168.1.100"
#define MQTT_PORT 1883
#define MQTT_USER "mqtt-username"
#define MQTT_PASSWORD "mqtt-password"

// OTA Password
#define OTA_PASSWORD "secure-ota-password"

// Porcupine (get from https://picovoice.ai/)
#define PORCUPINE_ACCESS_KEY "your-access-key"

// Weather API (get from https://openweathermap.org/)
#define WEATHER_API_KEY "your-api-key"

// Home Assistant
#define HA_BASE_URL "http://homeassistant.local:8123"
#define HA_TOKEN "your-long-lived-access-token"
```

### 4. Porcupine Integration

#### Download Porcupine
1. Get Porcupine Arduino SDK from: https://github.com/Picovoice/porcupine
2. Extract to `lib/porcupine/`
3. Download wake word `.ppn` files
4. Place `.ppn` files in `data/keywords/`

#### Available Wake Words
- Jarvis
- Alexa
- Computer
- Hey Google
- Or create custom wake words at https://console.picovoice.ai/

### 5. Build and Upload

#### Upload Filesystem (Web Files)
```bash
pio run --target uploadfs
```

#### Build and Upload Firmware
```bash
pio run --target upload
```

#### Monitor Serial Output
```bash
pio device monitor
```

### 6. First Boot Configuration

#### WiFi Setup
1. On first boot, device creates WiFi access point: `ESP32-EntryHub-Setup`
2. Connect to this network from your phone/computer
3. Captive portal opens automatically (or go to 192.168.4.1)
4. Enter your WiFi credentials
5. Device connects and restarts

#### Access Admin Panel
After WiFi configuration:
- URL: `http://entryhub.local` (mDNS)
- Or: `http://<device-ip>` (check serial monitor for IP)

## Admin Panel Usage

### Dashboard
- View system status and uptime
- Monitor WiFi signal strength
- Check voice recognition status
- Quick access to scenes
- Family presence overview

### Voice Control
- Configure wake word
- Adjust sensitivity
- Create custom voice commands
- Map commands to actions
- Test command recognition

### Presence Tracking
- Add family members
- View activity timeline
- Set up automation triggers
- Monitor arrival/departure

### Scenes
- Pre-configured scenes:
  - Welcome Home
  - Good Morning
  - Movie Time
  - Good Night
  - Away Mode
  - Party Mode
- Create custom scenes
- Trigger via voice or web

### Integrations
- **Home Assistant**: Auto-discovery via MQTT
- **Weather API**: OpenWeather integration
- **Calendar**: Google Calendar/iCal sync
- **MQTT**: Real-time communication

### Configuration
- Network settings
- System actions (reboot, reset)
- Firmware updates (OTA)
- Export/import configuration

## Home Assistant Integration

### Automatic Discovery
The device automatically publishes MQTT discovery messages to Home Assistant.

### Available Entities

#### Sensors
- `sensor.entryhub_voice_command` - Last voice command
- `sensor.entryhub_wifi_signal` - WiFi signal strength
- `sensor.entryhub_uptime` - Device uptime
- `sensor.entryhub_free_memory` - Available memory

#### Binary Sensors
- `binary_sensor.entryhub_voice_active` - Voice detection active
- `binary_sensor.entryhub_mqtt_connected` - MQTT connection status

#### Switches
- `switch.entryhub_welcome_home_scene`
- `switch.entryhub_good_night_scene`
- `switch.entryhub_away_mode_scene`

### Automation Example
```yaml
automation:
  - alias: "Entry Hub Voice Command"
    trigger:
      platform: state
      entity_id: sensor.entryhub_voice_command
    action:
      service: notify.mobile_app
      data:
        message: "Voice command received: {{ states('sensor.entryhub_voice_command') }}"
```

## Voice Commands

### Built-in Commands
- "Turn on/off the lights"
- "Lock/unlock the door"
- "Open/close the garage"
- "Good night" - Triggers good night scene
- "Welcome home" - Triggers welcome scene
- "What's the weather?"

### Adding Custom Commands
1. Go to Admin Panel > Voice Control
2. Click "Add Command"
3. Enter voice phrase
4. Select action type:
   - Scene trigger
   - Entity control
   - HTTP request
   - MQTT publish
5. Save and test

## OTA Updates

### Web OTA
1. Access `http://entryhub.local/update`
2. Select firmware file (.bin)
3. Click "Update"
4. Wait for completion and reboot

### Arduino OTA (PlatformIO)
```bash
pio run --target upload --upload-port entryhub.local
```

## Troubleshooting

### WiFi Issues
- Check serial output for error messages
- Reset WiFi: Admin Panel > Configuration > Reset WiFi
- Power cycle the device

### MQTT Not Connecting
- Verify broker IP and credentials in secrets.h
- Check broker is running: `mosquitto -v`
- Test with mosquitto_pub/sub

### Voice Not Working
- Check serial monitor for audio input
- Verify I2S wiring
- Test microphone: Admin Panel > Voice Control
- Adjust sensitivity slider

### Web Panel Not Loading
- Clear browser cache
- Check LittleFS uploaded: `pio run --target uploadfs`
- Verify at `http://<ip>/` not just `http://<ip>`

### Display Not Working
This is normal! Display support is placeholder.
See `display_manager.cpp` for instructions to add LVGL.

## API Reference

### REST API

#### GET /api/status
Returns system status
```json
{
  "device": {
    "name": "ESP32-EntryHub",
    "version": "1.0.0",
    "uptime": 12345,
    "free_heap": 200000
  },
  "wifi": {
    "connected": true,
    "ssid": "MyWiFi",
    "ip": "192.168.1.100",
    "rssi": -45
  }
}
```

#### GET /api/commands
Returns voice commands list

#### POST /api/commands
Add/update voice command
```json
{
  "command": "turn on bedroom light",
  "action": "homeassistant:light.bedroom",
  "category": "Lighting"
}
```

#### GET /api/presence
Returns presence information

#### POST /api/scene
Trigger a scene
```json
{
  "scene": "welcome_home"
}
```

### WebSocket

Connect to `ws://<device-ip>/ws`

#### Messages from Device
```json
{
  "type": "voice_detected",
  "wake_word": "jarvis",
  "timestamp": 12345
}
```

```json
{
  "type": "command_executed",
  "command": "turn on lights",
  "result": "success"
}
```

### MQTT Topics

#### Published by Device
- `entryhub/status` - Device status (JSON)
- `entryhub/voice/detected` - Wake word detected
- `entryhub/command/executed` - Command executed
- `entryhub/presence/status` - Presence updates

#### Subscribed by Device
- `entryhub/command` - Remote commands
- `entryhub/config` - Configuration updates
- `homeassistant/+/+/state` - HA entity states

## Advanced Configuration

### Custom Pin Configuration
Edit `include/config.h` to change pin assignments.

### Custom Scenes
Edit scenes in Admin Panel or modify source code in `src/main.cpp`.

### TensorFlow Lite Models
Place `.tflite` models in `data/models/` for command recognition.

## Performance Tips

1. Use 16MB Flash ESP32-S3 for best performance
2. Enable PSRAM for better memory handling
3. Adjust audio buffer sizes in config.h if needed
4. Reduce WebSocket broadcast frequency for stability
5. Use wired Ethernet for ultra-reliable connectivity

## Contributing

Please read CONTRIBUTING.md for contribution guidelines.

## License

MIT License - See LICENSE file

## Support

- GitHub Issues: [your-repo-url]/issues
- Documentation: [your-docs-url]
- Community Forum: [your-forum-url]
