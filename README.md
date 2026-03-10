# 🤖 ESP32 OLED Robot Face — 35 Modes Smart Display

An interactive **ESP32 OLED Robot Face project** featuring **35 different modes** including animations, clocks, mini games, and utilities.

The entire system is controlled using **just one push button** and displayed on a **0.96" SSD1306 OLED display**.

This project is designed for **embedded systems enthusiasts, robotics hobbyists, and IoT learners**.

---

# 🖥 Hardware Used

- ESP32 Development Board
- 0.96" SSD1306 OLED Display (128x64 I2C)
- Push Button
- Jumper Wires

---

# 🔌 Wiring

| Component | ESP32 Pin |
|-----------|-----------|
| OLED SDA | GPIO 21 |
| OLED SCL | GPIO 22 |
| Button | GPIO 13 |
| VCC | 3.3V |
| GND | GND |

---

# 🎮 Button Controls

| Action | Function |
|------|------|
| Short Press | Next mode / in-game action |
| Long Press | Main action (start / fire / turn) |
| Double Press | Always switch to next mode |

---

# 🧠 Features

✔ 35 interactive modes  
✔ Robot eye animations  
✔ Multiple clocks  
✔ Mini games  
✔ Cool OLED visual effects  
✔ Single button control system  
✔ WiFi NTP time synchronization  

---

# 🎨 Animation Modes

- Normal Eyes
- Blinking Eyes
- Happy Eyes
- Angry Eyes
- Sleepy Eyes
- Heart Eyes
- Sad Eyes
- Surprised Eyes
- Bouncing Ball
- Matrix Rain
- Radar Scan
- Plasma Animation
- Rain Animation
- Candle Flame
- Fireworks
- Starfield Warp

---

# ⏰ Clock Modes

- Digital Clock
- Analog Clock
- Binary Clock
- NTP Internet Time Sync

---

# 🛠 Utility Modes

- Stopwatch
- Countdown Timer
- Battery Display
- Thermometer
- Compass
- Spectrum Analyzer
- Oscilloscope
- Marquee Text

---

# 🎮 Mini Games

- Snake Game
- Pong Game
- Breakout
- Pac-Man Animation
- Dice Roller
- Space Shooter

---

# 🌐 WiFi Configuration

Edit the following lines in the code:

```cpp
#define WIFI_SSID   "YOUR_WIFI_NAME"
#define WIFI_PASS   "YOUR_WIFI_PASSWORD"
