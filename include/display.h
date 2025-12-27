#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <FastLED.h>

#include "buffer.h"
#include "parse.h"

#define PATTERN_DELAY 2000

#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32         
#define PANELS_NUMBER 2         

#define PANE_WIDTH PANEL_WIDTH * PANELS_NUMBER
#define PANE_HEIGHT PANEL_HEIGHT
#define NUM_LEDS PANE_WIDTH * PANE_HEIGHT

extern MatrixPanel_I2S_DMA *matrix;
extern CRGB *frontLedBuff;

void DisplayImage(void *pvParameters);
void initmatrix();
void drawText(int colorWheelOffset);
uint16_t colorWheel(uint8_t pos);
void mxfill(CRGB *leds);