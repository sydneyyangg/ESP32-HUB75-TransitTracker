// using fastled, cofnigure a buffer to send to dma 
#include "buffer.h"
#include "display.h"

CRGB *ledbuff = (CRGB *)malloc(NUM_LEDS * sizeof(CRGB));  

uint8_t color1 = 0, color2 = 0, color3 = 0;
uint16_t x,y;

void initbackbuffer(){
  buffclear(ledbuff);
}

void BufferTask(void *pvParameters){
  for (;;){
    // created random color gradient in ledbuff
        uint8_t color1 = 0;
        uint8_t color2 = random8();
        uint8_t color3 = 0;

        for (uint16_t i = 0; i<NUM_LEDS; ++i){
            ledbuff[i].r=color1++; // for each led, make the red go from 0 to (32*64 (numleds))
            ledbuff[i].g=color2; // " " ", " " green be the random color
            if (i % PANE_WIDTH == 0) 
            color3+=255/PANE_HEIGHT;

            ledbuff[i].b=color3;
        }
   }
}

void buffclear(CRGB *buf){
  memset(buf, 0x00, NUM_LEDS * sizeof(CRGB)); // flush buffer to black  
}
