#include "main.h"

TaskHandle_t DisplayImageHandle = NULL;
TaskHandle_t BufferHandle = NULL;
TaskHandle_t NetworkFetchHandle = NULL;

void setup(){

  Serial.begin(BAUD_RATE);
  Serial.println("Starting pattern test...");

    initmatrix();
    initbackbuffer();
    initwifi();
    init_time();

  xTaskCreatePinnedToCore(
    DisplayImage, // task function
    "DisplayImage", // task name
    4096, // stack size
    NULL, // params
    3, // priority
    &DisplayImageHandle, // task handle
    1
  );

  
  xTaskCreatePinnedToCore(
    BufferTask, // task function
    "BufferTask", // task name
    4096, // stack size
    NULL, // params
    2, // priority
    &BufferHandle, // task handle
    1
  );

  
  xTaskCreatePinnedToCore(
    NetworkTask, // task function
    "NetworkTask", // task name
    8192, // stack size
    NULL, // params
    3, // priority
    &NetworkFetchHandle, // task handle
    0
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
