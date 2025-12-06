# ESP32 Bell Controller

Automated bell controller with web interface for scheduling ring times.

## Features

- 🔔 Ring bell at scheduled times (day of week + time)
- 🌐 Web interface for configuration
- 📅 Multiple schedules support (up to 20)
- 🔘 Physical button for manual ring
- 💡 Built-in LED visual indicator when bell rings
- 🌍 WiFi connectivity with NTP time sync
- 💾 Persistent schedule storage

## Hardware Setup

### Required Components
- ESP32 development board
- Relay module (for bell control)
- Push button
- Bell/buzzer with appropriate power supply

### Pin Connections
- **GPIO 5**: Bell relay control (change in code if needed)
- **GPIO 4**: Physical button (with internal pullup)
- **GPIO 2**: Built-in LED (visual indicator, no wiring needed)

### Wiring
```
ESP32 GPIO 5 → Relay IN
Relay COM → Bell positive
Relay NO → Power supply positive
Bell negative → Power supply negative

ESP32 GPIO 4 → Button (one side)
Button (other side) → GND
```

## Software Setup

### 1. Install PlatformIO
Install PlatformIO IDE or CLI from https://platformio.org/

### 2. Configure WiFi
Edit `src/main.cpp` and change:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

### 3. Build and Upload
```bash
pio run --target upload
pio device monitor
```

### 4. Find IP Address
After uploading, open serial monitor to see the ESP32's IP address.

**Note:** The ESP32 will automatically use a static IP ending in `.215` on your local network. For example:
- If your network is `192.168.1.x`, the ESP32 will be at `192.168.1.215`
- If your network is `10.0.0.x`, the ESP32 will be at `10.0.0.215`

To change the last octet, edit `STATIC_IP_LAST_OCTET` in `src/main.cpp`.

## Usage

### Web Interface
1. Connect to the same WiFi network as ESP32
2. Open browser and navigate to ESP32's IP address
3. Current time is displayed at the top
4. Click "Ring Now!" to test the bell

### Adding Schedules
1. Select day of week (Sunday-Saturday)
2. Set hour (0-23) and minute (0-59)
3. Click "Add Schedule"
4. Schedule will appear in the list below

### Deleting Schedules
Click "Delete" button next to any schedule to remove it.

### Physical Button
Press the button connected to GPIO 4 to ring the bell manually.

## Configuration

### Change GPIO Pins
Edit these lines in `src/main.cpp`:
```cpp
const int BELL_PIN = 5;        // GPIO pin to control bell relay
const int BUTTON_PIN = 4;      // GPIO pin for physical button
```

### Change Bell Duration
Edit this line (time in milliseconds):
```cpp
const int BELL_DURATION = 1000; // Bell ring duration in ms
```

### Time Zone
Configure timezone through the web interface after uploading. The system supports automatic Daylight Saving Time transitions. No code changes needed.

## Troubleshooting

**WiFi not connecting:**
- Verify SSID and password are correct
- Check WiFi is 2.4GHz (ESP32 doesn't support 5GHz)

**Bell not ringing:**
- Check relay connections
- Verify GPIO pin number is correct
- Test with multimeter if relay is activating

**Time incorrect:**
- Configure timezone via web interface
- Verify internet connection for NTP sync
- Check that correct timezone is selected (supports automatic DST)

**Schedules not persisting:**
- ESP32 preferences stored in NVS flash
- If issues persist, may need to erase flash: `pio run --target erase`

## License
MIT License - feel free to modify and use as needed.
