# ESP32 Video Display System

Play any black and white video on ESP32 with SSD1306 OLED display.

**Original Project**: Based on [ESP32_BadApple by Hackerspace-FFM](https://github.com/hackffm/ESP32_BadApple)

![Video Display on ESP32](./images/star%20animation.png)

## Hardware Setup

### Required Components
- ESP32 development board
- SSD1306 OLED display (128x64)
- I2C connection: SDA to GPIO 21, SCL to GPIO 22

### Wiring
```
ESP32    OLED
3.3V  -> VCC
GND   -> GND
GPIO21 -> SDA
GPIO22 -> SCL
```

## Software Requirements
- Arduino IDE or PlatformIO
- ESP32 Arduino core
- Adafruit SSD1306 library

## Video Requirements
**IMPORTANT**: Videos must be black and white (monochrome) because the OLED is monochrome.

## Usage

1. Upload the main sketch
2. Upload data folder via "Tools" → "ESP32 Sketch Data Upload"

## Upload your own video

### 1. Install Required Libraries
- install python requirements: `pip install -r requirements.txt`
- setup platformio and install required libraries in platformio.ini

### 2. Convert Video
Edit `Compress.py` to point to your video:
```python
    video_path = Path("your_video_path.mp4")
```

Run the conversion:
```bash
python Compress.py
```

This creates `video.bin` file place into `data` folder.

### 3. Upload to ESP32
1. Upload the main sketch
2. Upload data via "Tools" → "ESP32 Sketch Data Upload"

## Credits
Based on [ESP32_BadApple](https://github.com/hackffm/ESP32_BadApple) by Hackerspace-FFM

## License
MIT License