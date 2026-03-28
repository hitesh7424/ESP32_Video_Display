// Bad Apple for ESP32 with OLED SSD1306 | 2018 by Hackerspace-FFM.de | MIT-License.
// Enhanced version with performance optimizations
#include "FS.h"
#include "SPIFFS.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "heatshrink_decoder.h"

// Board Boot SW GPIO 0
#define BOOT_SW 0

// Frame counter
#define ENABLE_FRAME_COUNTER

// Disable heatshrink error checking
#define DISABLE_HS_ERROR

// Hints:
// * Adjust the display pins below
// * After uploading to ESP32, also do "ESP32 Sketch Data Upload" from Arduino

// SSD1306 display I2C bus
#define I2C_SCL 22  // GPIO 22 (SCL)
#define I2C_SDA 21  // GPIO 21 (SDA)

#define OLED_BRIGHTNESS 16

// MAX freq for SCL is 4 MHz, However, Actual measured value is 892 kHz . (ESP32-D0WDQ6 (revision 1))
// see Inter-Integrated Circuit (I2C)
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html
#define I2C_SCLK_FREQ 4000000

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#if HEATSHRINK_DYNAMIC_ALLOC
#error HEATSHRINK_DYNAMIC_ALLOC must be false for static allocation test suite.
#endif

static heatshrink_decoder hsd;

// global storage for decodeRLE
int32_t runlength = -1;
int32_t c_to_dup = -1;

volatile unsigned long lastRefresh;
// 30 fps target rate = 33.333us
#define FRAME_DELAY_US 33333UL
#ifdef ENABLE_FRAME_COUNTER
int32_t frame = 0;
#endif

volatile bool isButtonPressing = false;

void ARDUINO_ISR_ATTR isr() {
    lastRefresh = micros();
    isButtonPressing = (digitalRead(BOOT_SW) == LOW);
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void putPixels(uint8_t c, int32_t len) {
  uint8_t b = 0x80;
  static int16_t curr_x = 0;
  static int16_t curr_y = 0;

  while(len--) {
    for(int i=0; i<8; i++) {
      if(c & b) {
        display.drawPixel(curr_x, curr_y, WHITE);  
      } else {
        display.drawPixel(curr_x, curr_y, BLACK); 
      }
      b >>= 1;
      curr_x++;
      if(curr_x >= 128) {
        curr_x = 0;
        curr_y++;
        if(curr_y >= 64) {
          curr_y = 0;
          display.display();
          
          // 30 fps target rate with microsecond precision
          if(!isButtonPressing) {
            lastRefresh += FRAME_DELAY_US;
#ifdef ENABLE_FRAME_COUNTER
            // Adjust 33.334us every 3 frame
            if ((++frame % 3) == 0) lastRefresh++;
#endif
            while(micros() < lastRefresh) ;
          }
        }
      }
    }
    b = 0x80;
  }
}

void decodeRLE(uint8_t c) {
    if(c_to_dup == -1) {
      if((c == 0x55) || (c == 0xaa)) {
        c_to_dup = c;
      } else {
        putPixels(c, 1);
      }
    } else {
      if(runlength == -1) {
        if(c == 0) {
          putPixels(c_to_dup & 0xff, 1);
          c_to_dup = -1;
        } else if((c & 0x80) == 0) {
          if(c_to_dup == 0x55) {
            putPixels(0, c);
          } else {
            putPixels(255, c);
          }
          c_to_dup = -1;
        } else {
          runlength = c & 0x7f;
        }
      } else {
        runlength = runlength | (c << 7);
          if(c_to_dup == 0x55) {
            putPixels(0, runlength);
          } else {
            putPixels(255, runlength);
          }
          c_to_dup = -1;  
          runlength = -1;        
      }
    }
}

#define RLEBUFSIZE 4096
#define READBUFSIZE 2048
void readFile(fs::FS &fs, const char * path){
    static uint8_t rle_buf[RLEBUFSIZE];
    size_t rle_bufhead = 0;
    size_t rle_size = 0; 
  
    size_t filelen = 0;
    size_t filesize;
    static uint8_t compbuf[READBUFSIZE];
    
    Serial.printf("Reading file: %s\n", path);
    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("Failed to open file for reading");
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0, 10);
        display.println("File open error.");
        display.setCursor(0, 20);
        display.println("Upload video.hs using");
        display.setCursor(0, 30);
        display.println("ESP32 Sketch Upload.");
        display.display();
        return;
    }
    filelen = file.size();
    filesize = filelen;
    Serial.printf("File size: %d\n", filelen);

    // init display, putPixels and decodeRLE
    display.clearDisplay();
    display.display();
    runlength = -1;
    c_to_dup = -1;   
    lastRefresh = micros();

    // init decoder
    heatshrink_decoder_reset(&hsd);
    size_t   count  = 0;
    uint32_t sunk   = 0;
    size_t toRead;
    size_t toSink = 0;
    uint32_t sinkHead = 0;
    

    // Go through file...
    while(filelen) {
      if(toSink == 0) {
        toRead = filelen;
        if(toRead > READBUFSIZE) toRead = READBUFSIZE;
        file.read(compbuf, toRead);
        filelen -= toRead;
        toSink = toRead;
        sinkHead = 0;
      }

      // uncompress buffer
      HSD_sink_res sres;
      sres = heatshrink_decoder_sink(&hsd, &compbuf[sinkHead], toSink, &count);
      //Serial.print("^^ sinked ");
      //Serial.println(count);      
      toSink -= count;
      sinkHead = count;        
      sunk += count;
      if (sunk == filesize) {
        heatshrink_decoder_finish(&hsd);
      }
        
      HSD_poll_res pres;
      do {
          rle_size = 0;
          pres = heatshrink_decoder_poll(&hsd, rle_buf, RLEBUFSIZE, &rle_size);
          //Serial.print("^^ polled ");
          //Serial.println(rle_size);
          if(pres < 0) {
            Serial.print("POLL ERR! ");
            Serial.println(pres);
            return;
          }

          rle_bufhead = 0;
          while(rle_size) {
            rle_size--;
            if(rle_bufhead >= RLEBUFSIZE) {
              Serial.println("RLE_SIZE ERR!");
              return;
            }
            decodeRLE(rle_buf[rle_bufhead++]);
          }
      } while (pres == HSDR_POLL_MORE);
    }
    file.close();
    Serial.println("Done.");
}



void setup(){
    Serial.begin(115200);
    Serial.println("ESP32 Bad Apple - Starting up...");
    
    Serial.println("Initializing OLED display...");
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 allocation failed");
        for(;;); // Don't proceed, loop forever
    }
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    
    // Test display with Hello World
    Serial.println("Testing display with Hello World...");
    display.setCursor(0, 0);
    display.println("Hello World!");
    display.setCursor(0, 16);
    display.println("I2C Address: 0x3C");
    display.setCursor(0, 32);
    display.println("SDA: GPIO 21");
    display.setCursor(0, 48);
    display.println("SCL: GPIO 22");
    display.display();
    
    delay(2000); // Show test screen for 2 seconds
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Mounting SPIFFS...");
    display.display();
    
    Serial.println("Mounting SPIFFS...");
    if(!SPIFFS.begin()){
        Serial.println("SPIFFS mount failed");
        display.clearDisplay();
        display.setCursor(0, 10);
        display.println("SPIFFS mount failed.");
        display.setCursor(0, 20);
        display.println("Upload video.hs using");
        display.setCursor(0, 30);
        display.println("ESP32 Sketch Upload.");
        display.display();
        return;
    }

    pinMode(0, INPUT_PULLUP);
    Serial.print("totalBytes(): ");
    Serial.println(SPIFFS.totalBytes());
    Serial.print("usedBytes(): ");
    Serial.println(SPIFFS.usedBytes());
    listDir(SPIFFS, "/", 0);
    readFile(SPIFFS, "/video.hs");

    //Serial.print("Format SPIFSS? (enter y for yes): ");
    // while(!Serial.available()) ;
    //if(Serial.read() == 'y') {
    //  bool ret = SPIFFS.format();
    //  if(ret) Serial.println("Success. "); else Serial.println("FAILED! ");
    //} else {
    //  Serial.println("Aborted.");
    //}
}

void loop(){

}