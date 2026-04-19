// ESP32 Video Player for SSD1306 OLED Display
// Supports simultaneous playback on:
// - 128x64 display on SDA 21 (video_128x64.bin)
// - 128x32 display on SDA 19 (video_128x32.bin)

#include "FS.h"
#include "SPIFFS.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#define SCREEN_WIDTH 128
#define I2C_SDA_128X64 21
#define I2C_SDA_128X32 19
#define I2C_SCL_128X64 22
#define I2C_SCL_128X32 18
#define OLED_ADDRESS 0x3C
#define I2C_CLOCK_HZ 400000

#define TARGET_FPS 30
#define FRAME_DELAY_MS (1000 / TARGET_FPS)

#define FRAME_WIDTH 128
#define MAX_FRAME_HEIGHT 64
#define MAX_BYTES_PER_FRAME (FRAME_WIDTH * MAX_FRAME_HEIGHT / 8)

TwoWire i2c64 = TwoWire(0);
TwoWire i2c32 = TwoWire(1);

Adafruit_SSD1306 display64(SCREEN_WIDTH, 64, &i2c64, -1);
Adafruit_SSD1306 display32(SCREEN_WIDTH, 32, &i2c32, -1);

struct PlaybackChannel
{
  const char* name;
  const char* preferredFile;
  int sdaPin;
  uint16_t frameHeight;
  uint16_t bytesPerFrame;
  Adafruit_SSD1306* display;
  TwoWire* bus;
  File videoFile;
  bool enabled;
  uint32_t totalFrames;
  uint32_t currentFrame;
  uint8_t frameBuffer[MAX_BYTES_PER_FRAME];
};

PlaybackChannel channel64 = {
  "128x64", "/video_128x64.bin", I2C_SDA_128X64, 64, (FRAME_WIDTH * 64 / 8), &display64, &i2c64, File(), false, 0, 0,
  { 0 }
};
PlaybackChannel channel32 = {
  "128x32", "/video_128x32.bin", I2C_SDA_128X32, 32, (FRAME_WIDTH * 32 / 8), &display32, &i2c32, File(), false, 0, 0,
  { 0 }
};

void renderFrame(PlaybackChannel& channel)
{
  channel.display->clearDisplay();

  for (uint16_t i = 0; i < channel.bytesPerFrame; i++)
  {
    uint8_t byte = channel.frameBuffer[i];
    uint16_t x = (i % (FRAME_WIDTH / 8)) * 8;
    uint16_t y = i / (FRAME_WIDTH / 8);

    if (y >= channel.frameHeight)
    {
      continue;
    }

    for (int bit = 7; bit >= 0; bit--)
    {
      if (byte & (1 << bit))
      {
        channel.display->drawPixel(x + (7 - bit), y, SSD1306_WHITE);
      }
    }
  }

  channel.display->display();
}

bool setupChannel(PlaybackChannel& channel)
{
  int sclPin = (channel.frameHeight == 64) ? I2C_SCL_128X64 : I2C_SCL_128X32;
  channel.bus->begin(channel.sdaPin, sclPin);
  channel.bus->setClock(I2C_CLOCK_HZ);
  delay(10);

  if (!channel.display->begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
  {
    Serial.print("[");
    Serial.print(channel.name);
    Serial.println("] Display not detected or init failed");
    return false;
  }

  channel.display->clearDisplay();
  channel.display->setTextSize(1);
  channel.display->setTextColor(SSD1306_WHITE);
  channel.display->setCursor(0, 0);
  channel.display->print("Init ");
  channel.display->println(channel.name);
  channel.display->display();

  channel.videoFile = SPIFFS.open(channel.preferredFile, "r");
  if (!channel.videoFile)
  {
    Serial.print("[");
    Serial.print(channel.name);
    Serial.print("] Missing file: ");
    Serial.println(channel.preferredFile);

    channel.display->clearDisplay();
    channel.display->setCursor(0, 0);
    channel.display->print(channel.name);
    channel.display->setCursor(0, 10);
    channel.display->println("No video file");
    channel.display->display();
    return false;
  }

  const uint32_t fileSize = channel.videoFile.size();
  channel.totalFrames = fileSize / channel.bytesPerFrame;
  channel.currentFrame = 0;
  channel.enabled = true;

  Serial.print("[");
  Serial.print(channel.name);
  Serial.print("] File: ");
  Serial.print(channel.preferredFile);
  Serial.print(", size: ");
  Serial.print(fileSize);
  Serial.print(" bytes, frames: ");
  Serial.println(channel.totalFrames);
  Serial.print("[");
  Serial.print(channel.name);
  Serial.print("] I2C pins SDA=");
  Serial.print(channel.sdaPin);
  Serial.print(" SCL=");
  Serial.print(sclPin);
  Serial.print(" @ ");
  Serial.print(I2C_CLOCK_HZ);
  Serial.println(" Hz");

  channel.display->clearDisplay();
  channel.display->setCursor(0, 0);
  channel.display->print("Ready ");
  channel.display->println(channel.name);
  channel.display->setCursor(0, 10);
  channel.display->print("Frames: ");
  channel.display->println(channel.totalFrames);
  channel.display->display();

  return true;
}

void playChannelFrame(PlaybackChannel& channel)
{
  if (!channel.enabled)
  {
    return;
  }

  if (channel.videoFile.available() < channel.bytesPerFrame)
  {
    channel.videoFile.seek(0);
    channel.currentFrame = 0;
  }

  int bytesRead = channel.videoFile.read(channel.frameBuffer, channel.bytesPerFrame);
  if (bytesRead != channel.bytesPerFrame)
  {
    channel.videoFile.seek(0);
    channel.currentFrame = 0;
    return;
  }

  renderFrame(channel);
  channel.currentFrame++;
}

void drawLoadingScreen(PlaybackChannel& channel, uint8_t progressPercent)
{
  if (!channel.enabled)
  {
    return;
  }

  const int barX = 8;
  const int barWidth = SCREEN_WIDTH - (barX * 2);
  const int barHeight = 8;
  const int textY = 8;
  const int barY = (channel.frameHeight <= 32) ? 20 : 40;
  int filledWidth = (barWidth * progressPercent) / 100;
  if (filledWidth < 0)
  {
    filledWidth = 0;
  }
  if (filledWidth > barWidth)
  {
    filledWidth = barWidth;
  }

  channel.display->clearDisplay();
  channel.display->setTextSize(1);
  channel.display->setTextColor(SSD1306_WHITE);
  channel.display->setCursor(0, textY);
  channel.display->print("Loading ");
  channel.display->print(progressPercent);
  channel.display->print("%");

  channel.display->drawRect(barX, barY, barWidth, barHeight, SSD1306_WHITE);
  if (filledWidth > 0)
  {
    channel.display->fillRect(barX + 1, barY + 1, filledWidth - 2, barHeight - 2, SSD1306_WHITE);
  }
  channel.display->display();
}

void showStartupLoadingScreen()
{
  for (uint8_t progress = 0; progress <= 100; progress += 10)
  {
    drawLoadingScreen(channel64, progress);
    drawLoadingScreen(channel32, progress);
    delay(100);
  }
}

void setup()
{
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n=== ESP32 Dual OLED Video Player ===");

  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS mount failed");
    while (1)
    {
      delay(1000);
    }
  }

  bool has64 = setupChannel(channel64);
  bool has32 = setupChannel(channel32);

  if (!has64 && !has32)
  {
    Serial.println("No active channels. Need display + matching video file.");
    while (1)
    {
      delay(1000);
    }
  }

  showStartupLoadingScreen();
  Serial.println("Playback started");
}

void loop()
{
  static uint32_t lastFrameTime = 0;
  uint32_t now = millis();
  if (now - lastFrameTime < FRAME_DELAY_MS)
  {
    return;
  }
  lastFrameTime = now;

  playChannelFrame(channel64);
  playChannelFrame(channel32);

  static uint32_t logCounter = 0;
  logCounter++;
  if (logCounter % 30 == 0)
  {
    if (channel64.enabled)
    {
      Serial.print("[128x64] ");
      Serial.print(channel64.currentFrame);
      Serial.print("/");
      Serial.println(channel64.totalFrames);
    }
    if (channel32.enabled)
    {
      Serial.print("[128x32] ");
      Serial.print(channel32.currentFrame);
      Serial.print("/");
      Serial.println(channel32.totalFrames);
    }
  }
}