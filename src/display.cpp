// propogate to led drivers on display
#include "display.h"

MatrixPanel_I2S_DMA *matrix = nullptr;

//char *str = "* ESP32 I2S DMA *";
char str[32];
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

        Serial.println("ClearScreen()");
        matrix->clearScreen();
        delay(500);

        Serial.printf("Minutes until: %d min\n", minutes_until);
        matrix->clearScreen();
        drawText(0);
        
        Serial.println("\n====\n");

        // take a rest for a while
        vTaskDelay(15000 / portTICK_PERIOD_MS);
        
        UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
        Serial.printf("DisplayTask stack free: %u bytes\n", watermark * sizeof(StackType_t));
    }
}
// 0x666d

#ifdef NO_GFX
void drawText(int colorWheelOffset){}
#else
void drawText(int colorWheelOffset){
  // draw routes
  matrix->setTextSize(1);     // size 1 == 8 pixels high
  matrix->setTextWrap(false); // Don't wrap at end of line - will do ourselves
  matrix->setTextColor(0x666d);

  uint8_t w = 0;

  snprintf(str, sizeof(str), "201");

  matrix->setCursor(5, 5);    // start at top left, with 5,5 pixel of spacing
  for (w=0; w<strlen(str); w++) {
    matrix->print(str[w]);
  }

  matrix->setCursor(5, 18);
  for (w=0; w<strlen(str); w++) {
      matrix->print(str[w]);
  }

  matrix->setCursor(96, 5);
  snprintf(str, sizeof(str), "%d min", minutes_until);
  for (w=0; w<strlen(str); w++) {
      matrix->print(str[w]);
  }

  snprintf(str, sizeof(str), "Con. College");
  matrix->setCursor(21, 5);
  matrix->setTextColor(0xffff);
  for (w=0; w<strlen(str); w++) {
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

void displayBufferImage(){
  // Clearing CRGB ledbuff
  // buffclear(frontLedBuff);

  // //propogate back buff to front 
  // // memcpy(frontLedBuff, ledbuff, NUM_LEDS * sizeof(CRGB));
  
  // //display on screen 
  // mxfill(frontLedBuff);
  // Serial.println("LedBuffer MxFill");
  // vTaskDelay(500 / portTICK_PERIOD_MS);
}
