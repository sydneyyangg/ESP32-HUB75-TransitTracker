#include <Arduino.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif_net_stack.h"
#include "lwip/netif.h"
#include "driver/gpio.h"

// WiFi Configuration

// Built-in LED (many ESP32 dev boards use GPIO2)
#define BLINK_GPIO GPIO_NUM_2

// Event bits for WiFi
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

void NetworkTask(void *pvParameters);
void initwifi();
esp_err_t wifi_init_sta(void);