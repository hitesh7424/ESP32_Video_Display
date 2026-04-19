# ESP32 Video Display System

Play black-and-white video on ESP32 with SSD1306 OLED displays.

**Original Project**: Based on [ESP32_BadApple by Hackerspace-FFM](https://github.com/hackffm/ESP32_BadApple)

![Video Display on ESP32](./images/star%20animation.png)

## Hardware Setup

### Required Components

- ESP32 development board
- SSD1306 OLED display (128x32 or 128x64)
- I2C connection: SDA to GPIO 21, SCL to GPIO 22

### Wiring

```text
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
- Python 3
- OpenCV (`opencv-python`)

## Video Requirements

**IMPORTANT**: Videos must be black and white (monochrome) because the OLED is monochrome.

Recommended: keep source videos low resolution and short duration to reduce output `.bin` size.

## Usage

1. Upload the main sketch
2. Upload data folder via "Tools" → "ESP32 Sketch Data Upload"

## Upload your own video

### 1. Install Required Libraries

- install python requirements: `pip install -r requirements.txt`
- setup platformio and install required libraries in platformio.ini

### 2. Convert Video

Use the converter script directly with arguments:

```bash
python compress.py --video "./videos/your_video.mp4"
```

Default run (no `--size`) generates both OLED formats:

- `./data/video_128x32.bin`
- `./data/video_128x64.bin`

Generate only one format:

```bash
python compress.py --video "./videos/your_video.mp4" --size 128x64 --output ./data/video.bin
```

Generate multiple formats with custom base output name:

```bash
python compress.py --video "./videos/your_video.mp4" --size 128x32 --size 128x64 --output ./data/video.bin
```

When multiple sizes are requested, output files are auto-suffixed:

- `video_128x32.bin`
- `video_128x64.bin`

### 3. Upload to ESP32

1. Upload the main sketch
2. Upload data via "Tools" → "ESP32 Sketch Data Upload"

```bash
platformio run --target uploadfs --environment esp32doit-devkit-v1
```

Make sure the firmware reads the same `.bin` filename you generated.

## Credits

Based on [ESP32_BadApple](https://github.com/hackffm/ESP32_BadApple) by Hackerspace-FFM

## License

MIT License
