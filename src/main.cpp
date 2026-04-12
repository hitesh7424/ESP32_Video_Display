// ESP32 Video Player for SSD1306 OLED Display
// Plays 128x64 1-bit video at 30 FPS
// Video format: raw binary, 1024 bytes per frame (128x64 bits)

#include "FS.h"
#include "SPIFFS.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

// Display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define I2C_SDA 21
#define I2C_SCL 22
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Frame timing
#define TARGET_FPS 30
#define FRAME_DELAY_MS (1000 / TARGET_FPS)  // ~33ms per frame

// Video parameters
#define FRAME_WIDTH 128
#define FRAME_HEIGHT 64
#define BYTES_PER_FRAME (FRAME_WIDTH * FRAME_HEIGHT / 8)  // 1024 bytes

// File handle
File videoFile;
uint32_t totalFrames = 0;
uint32_t currentFrame = 0;

void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println("\n\n=== ESP32 Video Player ===");
    
    // Initialize I2C
    Wire.begin(I2C_SDA, I2C_SCL);
    
    // Initialize display
    Serial.println("Initializing display...");
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println("SSD1306 allocation failed");
        while(1);
    }
    
    // Show startup message
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Initializing...");
    display.display();
    
    // Initialize SPIFFS
    Serial.println("Mounting SPIFFS...");
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed");
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("SPIFFS mount failed!");
        display.display();
        while(1);
    }
    
    // List files
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    Serial.println("\nFiles in SPIFFS:");
    while (file) {
        Serial.print("  ");
        Serial.print(file.name());
        Serial.print(" - ");
        Serial.print(file.size());
        Serial.println(" bytes");
        file = root.openNextFile();
    }
    
    // Open video file
    Serial.println("\nOpening video file...");
    videoFile = SPIFFS.open("/video.bin", "r");
    if (!videoFile) {
        Serial.println("Failed to open /video.bin");
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("video.bin not found!");
        display.display();
        while(1);
    }
    
    // Calculate total frames
    uint32_t fileSize = videoFile.size();
    totalFrames = fileSize / BYTES_PER_FRAME;
    
    Serial.println("\n=== Video Info ===");
    Serial.print("File size: ");
    Serial.print(fileSize);
    Serial.println(" bytes");
    Serial.print("Total frames: ");
    Serial.println(totalFrames);
    Serial.print("Duration: ");
    Serial.print(totalFrames / (float)TARGET_FPS);
    Serial.println(" seconds");
    Serial.print("Frame rate: ");
    Serial.print(TARGET_FPS);
    Serial.println(" FPS");
    
    // Show ready message
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Video ready!");
    display.setCursor(0, 10);
    display.print("Frames: ");
    display.println(totalFrames);
    display.setCursor(0, 20);
    display.print("Size: ");
    display.print(fileSize / 1024);
    display.println(" KB");
    display.display();
    
    delay(2000);
    
    Serial.println("\nStarting playback...");
}

void displayFrame(uint8_t* frameData) {
    // Clear display buffer
    display.clearDisplay();
    
    // Draw each byte as 8 pixels
    for (uint16_t i = 0; i < BYTES_PER_FRAME; i++) {
        uint8_t byte = frameData[i];
        
        // Calculate x, y position
        // Each byte represents 8 horizontal pixels
        uint16_t x = (i % (FRAME_WIDTH / 8)) * 8;
        uint16_t y = i / (FRAME_WIDTH / 8);
        
        // Draw 8 pixels from this byte
        for (int bit = 7; bit >= 0; bit--) {
            if (byte & (1 << bit)) {
                display.drawPixel(x + (7 - bit), y, SSD1306_WHITE);
            }
        }
    }
    
    // Update display
    display.display();
}

void loop() {
    static uint32_t lastFrameTime = 0;
    uint32_t currentTime = millis();
    
    // Check if it's time for next frame
    if (currentTime - lastFrameTime < FRAME_DELAY_MS) {
        return;  // Not time yet
    }
    
    lastFrameTime = currentTime;
    
    // Read frame data
    static uint8_t frameBuffer[BYTES_PER_FRAME];
    
    if (videoFile.available() >= BYTES_PER_FRAME) {
        // Read frame
        int bytesRead = videoFile.read(frameBuffer, BYTES_PER_FRAME);
        
        if (bytesRead == BYTES_PER_FRAME) {
            // Display frame
            displayFrame(frameBuffer);
            
            currentFrame++;
            
            // Print progress every 30 frames (~1 second)
            if (currentFrame % 30 == 0) {
                Serial.print("Frame ");
                Serial.print(currentFrame);
                Serial.print("/");
                Serial.print(totalFrames);
                Serial.print(" (");
                Serial.print((currentFrame * 100) / totalFrames);
                Serial.println("%)");
            }
        }
    } else {
        // End of video - loop or stop
        Serial.println("\nEnd of video - looping...");
        
        // Reset to beginning
        videoFile.seek(0);
        currentFrame = 0;
        
        // Show message
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Video looping...");
        display.display();
        delay(2000);
    }
}