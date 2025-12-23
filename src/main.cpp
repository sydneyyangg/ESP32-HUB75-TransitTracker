#ifdef IDF_BUILD
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include "sdkconfig.h"
#endif

#include <Arduino.h>
#include "xtensa/core-macros.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "main.h"
#include "display.h"
#include "buffer.h"
#include "network.h"

// HUB75E pinout
// R1 | G1
// B1 | GND
// R2 | G2
// B2 | E
//  A | B
//  C | D
// CLK| LAT
// OE | GND


TaskHandle_t DisplayImageHandle = NULL;
TaskHandle_t BufferHandle = NULL;
TaskHandle_t NetworkFetchHandle = NULL;

void setup(){

  Serial.begin(BAUD_RATE);
  Serial.println("Starting pattern test...");

    initmatrix();
    initbackbuffer();
    initwifi();

  //could pin to core to put wifi on a core
  xTaskCreate(
    DisplayImage, // task function
    "DisplayImage", // task name
    2048, // stack size
    NULL, // params
    3, // priority
    &DisplayImageHandle // task handle
  );

  
  xTaskCreate(
    BufferTask, // task function
    "BufferTask", // task name
    2048, // stack size
    NULL, // params
    2, // priority
    &BufferHandle // task handle
  );

  
  xTaskCreate(
    NetworkTask, // task function
    "NetworkTask", // task name
    2048, // stack size
    NULL, // params
    3, // priority
    &NetworkFetchHandle // task handle
  );

}

void loop(){}

/**
 *  The one for 256+ matrices
 *  otherwise this:
 *    for (uint8_t i = 0; i < MATRIX_WIDTH; i++) {}
 *  turns into an infinite loop
//  */
// uint16_t XY16( uint16_t x, uint16_t y)
// { 
//   if (x<PANE_WIDTH && y < PANE_HEIGHT){
//     return (y * PANE_WIDTH) + x;
//   } else {
//     return 0;
//   }
// }
