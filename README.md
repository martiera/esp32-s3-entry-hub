# ESP32-S3 Entry Hub

A sophisticated, voice-controlled smart home entry panel with web-based administration.

## Features

### Core Functionality
- 🎤 **Voice Control**: Wake word detection using Porcupine + custom voice commands
- 🌐 **WiFi & MQTT**: Seamless connectivity with auto-reconnect
- 🔄 **OTA Updates**: Over-the-air firmware updates (web & ArduinoOTA)
- 💾 **Persistent Storage**: Configuration saved in LittleFS

### Voice & Audio
- I2S INMP441 microphone integration
- Porcupine wake word detection (customizable wake words)
- TensorFlow Lite Micro for command recognition
- Real-time audio processing

### Web Admin Panel
- 🎨 Modern, responsive design
- ⚙️ System configuration
- 🎯 Wake word management
- 📋 Voice command administration
- 👥 Family member presence tracking
- 🌤️ Weather widget integration
- 📅 Calendar sync
- 🏠 Home Assistant entity control
- 💡 Scene control (lights, locks, garage)

### Home Assistant Integration
- MQTT auto-discovery
- Entity state monitoring
- Scene and automation triggers
- Real-time status updates

### Future Enhancements
- 🖥️ IPS touchscreen with LVGL UI
- Advanced gesture controls
- Multi-language support

## Hardware Requirements

- **MCU**: ESP32-S3-WROOM-1 (16MB Flash, 8MB PSRAM recommended)
- **Microphone**: INMP441 I2S Digital Microphone
- **Display** (optional): IPS touchscreen with capacitive touch
- **Power**: 5V USB-C or external power supply

## Pin Configuration

### I2S INMP441 Microphone
- SCK (Serial Clock): GPIO 4
- WS (Word Select): GPIO 5
- SD (Serial Data): GPIO 6

### SPI Display (for future LVGL)
- MOSI: GPIO 11
- MISO: GPIO 13
- SCK: GPIO 12
- CS: GPIO 10
- DC: GPIO 9
- RST: GPIO 8
- Backlight: GPIO 7

### Touch Controller (I2C)
- SDA: GPIO 17
- SCL: GPIO 18

## Setup Instructions

1. **Install PlatformIO**
   - Install VS Code
   - Install PlatformIO IDE extension

2. **Clone and Open Project**
   ```bash
   git clone <repository-url>
   cd esp32-s3-entry-hub
   code .
   ```

3. **Configure Secrets**
   - Copy `include/secrets.h.example` to `include/secrets.h`
   - Fill in your WiFi and MQTT credentials

4. **Upload Filesystem**
   ```bash
   pio run --target uploadfs
   ```

5. **Build and Upload**
   ```bash
   pio run --target upload
   ```

6. **Access Admin Panel**
   - Connect to WiFi network "ESP32-EntryHub-Setup" (first time)
   - Configure WiFi credentials
   - Access panel at `http://entryhub.local` or device IP

## Web Admin Panel Features

### Dashboard
- System status and uptime
- Network information
- Voice recognition status
- Quick actions

### Configuration
- WiFi settings
- MQTT broker configuration
- Wake word selection
- Audio sensitivity tuning

### Voice Commands
- Create custom voice commands
- Map commands to actions
- Test command recognition
- Import/export command sets

### Presence Tracking
- Family member status
- Arrival/departure notifications
- Location-based automation triggers

### Integrations
- Weather API configuration
- Calendar sync (Google/iCal)
- Home Assistant connection
- Custom webhooks

### Scene Control
- Light control
- Door lock management
- Garage door control
- Custom scenes

## API Endpoints

### REST API
- `GET /api/status` - System status
- `GET /api/config` - Current configuration
- `POST /api/config` - Update configuration
- `GET /api/commands` - List voice commands
- `POST /api/commands` - Add/update command
- `DELETE /api/commands/:id` - Remove command
- `GET /api/presence` - Family member status
- `POST /api/scene/:name` - Trigger scene

### WebSocket
- Real-time voice recognition feedback
- Live system status updates
- Presence change notifications

## MQTT Topics

### Publish
- `entryhub/status` - Device status
- `entryhub/voice/detected` - Wake word detected
- `entryhub/command/executed` - Command executed
- `entryhub/presence/status` - Presence updates

### Subscribe
- `entryhub/command` - Remote commands
- `entryhub/config` - Configuration updates
- `homeassistant/+/+/state` - HA entity states

## Development

### Project Structure
```
esp32-s3-entry-hub/
├── include/            # Header files
├── src/               # Source code
│   ├── main.cpp       # Main application
│   ├── wifi_manager.cpp
│   ├── mqtt_client.cpp
│   ├── audio_handler.cpp
│   ├── porcupine_handler.cpp
│   ├── tflm_handler.cpp
│   ├── web_server.cpp
│   └── ha_integration.cpp
├── data/              # Web files (HTML/CSS/JS)
├── lib/               # Custom libraries
└── test/              # Unit tests
```

### Building
```bash
# Build
pio run

# Upload firmware
pio run --target upload

# Upload filesystem
pio run --target uploadfs

# Monitor serial
pio device monitor
```

## License

MIT License - See LICENSE file

## Contributing

Pull requests welcome! Please read CONTRIBUTING.md first.

## Credits

- Porcupine Wake Word Engine by Picovoice
- TensorFlow Lite Micro
- LVGL Graphics Library
- Home Assistant Community
