#pragma once

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <FastLED.h>

// gradient buffer
extern CRGB *ledbuff;
//

void initbackbuffer();
void BufferTask(void *pvParameters);
void buffclear(CRGB *buf);
