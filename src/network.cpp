// configure and host wifi connection with esp and device
// fetch http rest api from transit
// parse for data
// store to send to ui task
#include "network.h"

static EventGroupHandle_t s_wifi_event_group; // This is set as either the fail or connect bit to enable wifi init.
static int s_retry_num = 0;

void NetworkTask(void *pvParameters){
    for (;;){
        // connect to wifi
        // fetch data
        // parse data
        ESP_ERROR_CHECK(esp_wifi_stop());
        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
}

void initwifi()
{
    Serial.println("ESP32 Servo Tracker Starting...");

    // Initialize built-in LED GPIO for blink endpoint
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BLINK_GPIO, 0);

    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs to be erased, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    Serial.println("Initializing WiFi...");
    ret = wifi_init_sta();
    
    if (ret != ESP_OK) {
        Serial.print("WiFi initialization failed! Restarting in 10 seconds...");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        esp_restart();
        return;
    }
}

// WiFi event handler, when WiFi or IP events occur
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{   // if WiFi station interface has started
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect(); // Kick off actual connection process
        Serial.print("Connecting to WiFi...");
        // if WiFi wifi station got disconnected 
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            Serial.printf("Retry connecting to WiFi... (%d/%d)", s_retry_num, MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            Serial.printf("Failed to connect to WiFi after %d attempts.\n", MAX_RETRY);
        }
    //Got an IP address and received network config from DHCP
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data; // calls event to read the iP address
        Serial.printf("Connected! IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Initialize WiFi
esp_err_t wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate(); // creates event group and assigns handle. signal success/fail for init
    
    ESP_ERROR_CHECK(esp_netif_init()); // error check network interface abstr. layer
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // error check loop for ESP-IDF system events
    esp_netif_create_default_wifi_sta(); // ties WiFi driver to network stack

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // init wifi driver
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); // check for errors with init

    // Register event handlers
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Configure WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold = {
                .authmode = WIFI_AUTH_WPA2_PSK,
            }
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    Serial.println("WiFi initialization complete");
    
    // Wait for connection or failure
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        Serial.printf("Connected to SSID: %s", WIFI_SSID);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        Serial.printf("Failed to connect to SSID: %s", WIFI_SSID);
        return ESP_FAIL;
    } else {
        Serial.println("Unexpected WiFi event");
        return ESP_FAIL;
    }
}

// define lwip ipv6 hook to do nothing
// needed to patch esp-idf and arduino environment compatibility
// pre v. 3.2.0 (link)
#if CONFIG_LWIP_HOOK_IP6_INPUT_CUSTOM
extern "C" int lwip_hook_ip6_input(struct pbuf *p, struct netif *inp) __attribute__((weak));
extern "C" int lwip_hook_ip6_input(struct pbuf *p, struct netif *inp) {
  if (ip6_addr_isany_val(inp->ip6_addr[0].u_addr.ip6)) {
    // We don't have an LL address -> eat this packet here, so it won't get accepted on input netif
    pbuf_free(p);
    return 1;
  }
  return 0;
}
#endif