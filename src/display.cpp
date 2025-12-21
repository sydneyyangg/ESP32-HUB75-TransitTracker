#include "display.h"
#include "buffer.h"

MatrixPanel_I2S_DMA *matrix = nullptr;

const char *str = "* ESP32 I2S DMA *";
CRGB *frontLedBuff = (CRGB *)malloc(NUM_LEDS * sizeof(CRGB));  

void initmatrix(){
  //HUB75_I2S_CFG::i2s_pins _pins={R1, G1, BL1, R2, G2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};
  
  HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER);
  mxconfig.clkphase = false;
  matrix = new MatrixPanel_I2S_DMA(mxconfig);
  //resets buffers, configs shift driver
  matrix->begin();
  matrix->setBrightness8(100);
  matrix->setLatBlanking(2);

  buffclear(frontLedBuff);
}

void DisplayImage(void *pvParameters){
    for (;;){
        Serial.begin(115200);

        Serial.print("ClearScreen()");
        matrix->clearScreen();
        delay(PATTERN_DELAY);

        Serial.println("Fill screen: RED");
        matrix->fillScreenRGB888(255, 0, 0);
        delay(PATTERN_DELAY);

        Serial.println("Fill screen: GREEN");
        matrix->fillScreenRGB888(0, 255, 0);
        delay(PATTERN_DELAY);

        Serial.println("Fill screen: BLUE");
        matrix->fillScreenRGB888(0, 0, 255);
        delay(PATTERN_DELAY);
         
        // Clearing CRGB ledbuff
        buffclear(frontLedBuff);

        //propogate back buff to front 
        memcpy(frontLedBuff, ledbuff, NUM_LEDS * sizeof(CRGB));
        
        //display on screen 
        mxfill(frontLedBuff);
        Serial.printf("LedBuffer MxFill");
        delay(PATTERN_DELAY);

        Serial.printf("Text display");
        matrix->clearScreen();
        drawText(0);
        delay(PATTERN_DELAY);
        
        Serial.println("\n====\n");

        // take a rest for a while
        delay(10000);
    }
}


#ifdef NO_GFX
void drawText(int colorWheelOffset){}
#else
void drawText(int colorWheelOffset){
  // draw some text
  matrix->setTextSize(1);     // size 1 == 8 pixels high
  matrix->setTextWrap(false); // Don't wrap at end of line - will do ourselves

  matrix->setCursor(5, 5);    // start at top left, with 5,5 pixel of spacing
  uint8_t w = 0;

  for (w=0; w<strlen(str); w++) {
    matrix->setTextColor(colorWheel((w*32)+colorWheelOffset));
    matrix->print(str[w]);
  }
}
#endif

uint16_t colorWheel(uint8_t pos) {
  if(pos < 85) {
    return matrix->color565(pos * 3, 255 - pos * 3, 0);
  } else if(pos < 170) {
    pos -= 85;
    return matrix->color565(255 - pos * 3, 0, pos * 3);
  } else {
    pos -= 170;
    return matrix->color565(0, pos * 3, 255 - pos * 3);
  }
}

void IRAM_ATTR mxfill(CRGB *leds){
  uint16_t y = PANE_HEIGHT;
  do {
    --y;
    uint16_t x = PANE_WIDTH;
    do {
      --x;
        uint16_t _pixel = y * PANE_WIDTH + x;
        matrix->drawPixelRGB888( x, y, leds[_pixel].r, leds[_pixel].g, leds[_pixel].b);
    } while(x);
  } while(y);
}


