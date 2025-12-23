#include <Arduino.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif_net_stack.h"
#include "lwip/netif.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

// WiFi Configuration
//#define WIFI_SSID      "Toby"
//#define WIFI_PASS      "Micah6JMH"  // Empty for open network, or set password for secured network
#define WIFI_SSID      "Testing"
#define WIFI_PASS      "12312312"  // Empty for open network, or set password for secured network
#define MAX_RETRY      5
// Built-in LED (many ESP32 dev boards use GPIO2)
#define BLINK_GPIO GPIO_NUM_2

// Event bits for WiFi
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

void NetworkTask(void *pvParameters);
void initwifi();
esp_err_t wifi_init_sta(void);
static httpd_handle_t start_webserver(void);
