// configure and host wifi connection with esp and device
// fetch http rest api from transit
// parse for data
// store to send to ui task
#include "network.h"

static EventGroupHandle_t s_wifi_event_group; // This is set as either the fail or connect bit to enable wifi init.
static int s_retry_num = 0;

void NetworkTask(void *pvParameters){
    for (;;){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void initwifi()
{
    Serial.print("ESP32 Servo Tracker Starting...");

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
    Serial.print("Initializing WiFi...");
    ret = wifi_init_sta();
    
    if (ret != ESP_OK) {
        Serial.print("WiFi initialization failed! Restarting in 10 seconds...");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        esp_restart();
        return;
    }
// Start web server
    httpd_handle_t server = start_webserver();
    if (server == NULL) {
        Serial.print("Failed to start web server! Restarting in 10 seconds...");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        esp_restart();
        return;
    }

    Serial.print("Web server started successfully!");
    Serial.print("Ready to receive commands at http://<ESP32_IP>/move?direction=<left|right|up|down|center>");
    
}


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
/*
// Scan for available WiFi networks
void wifi_scan(void)
{
    ESP_LOGI(TAG, "Starting WiFi scan...");
    
    uint16_t number = 10;
    wifi_ap_record_t ap_info[10];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    
    ESP_LOGI(TAG, "Found %d WiFi networks:", ap_count);
    for (int i = 0; (i < 10) && (i < ap_count); i++) {
        ESP_LOGI(TAG, "  %d: %s (RSSI: %d, Auth: %d, Channel: %d)", 
                 i+1, ap_info[i].ssid, ap_info[i].rssi, 
                 ap_info[i].authmode, ap_info[i].primary);
    }
}
*/
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

    ESP_LOGI(TAG, "WiFi initialization complete");
    
    // Scan for networks before connecting
    //wifi_scan();

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
        Serial.print("Unexpected WiFi event");
        return ESP_FAIL;
    }
}

// HTTP GET handler for root "/"
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char* resp_str = "ESP32 Servo Tracker Ready\n";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// HTTP GET handler for "/move"
static esp_err_t move_get_handler(httpd_req_t *req)
{
    char query[100]; // holds URL query string (after ?)
    char direction[20] = {0}; // holds direction query param (e.g. left)
    
    // Get query string
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        // Extract "direction" parameter
        if (httpd_query_key_value(query, "direction", direction, sizeof(direction)) == ESP_OK) {
            ESP_LOGI(TAG, "Received command: %s", direction);
            
            // Process command
            if (strcmp(direction, "left") == 0) {
                ESP_LOGI(TAG, "Moving LEFT");
                // TODO: Add servo control code here
                httpd_resp_send(req, "Moved left", HTTPD_RESP_USE_STRLEN);
            }
            else if (strcmp(direction, "right") == 0) {
                ESP_LOGI(TAG, "Moving RIGHT");
                // TODO: Add servo control code here
                httpd_resp_send(req, "Moved right", HTTPD_RESP_USE_STRLEN);
            }
            else if (strcmp(direction, "up") == 0) {
                ESP_LOGI(TAG, "Moving UP");
                // TODO: Add servo control code here
                httpd_resp_send(req, "Moved up", HTTPD_RESP_USE_STRLEN);
            }
            else if (strcmp(direction, "down") == 0) {
                ESP_LOGI(TAG, "Moving DOWN");
                // TODO: Add servo control code here
                httpd_resp_send(req, "Moved down", HTTPD_RESP_USE_STRLEN);
            }
            else if (strcmp(direction, "center") == 0) {
                ESP_LOGI(TAG, "Moving to CENTER");
                // TODO: Add servo control code here
                httpd_resp_send(req, "Centered", HTTPD_RESP_USE_STRLEN);
            }
            else {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid direction");
                return ESP_FAIL;
            }
            
            return ESP_OK;
        }
    }
    
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing direction parameter");
    return ESP_FAIL;
}

// HTTP GET handler for "/blink" - blinks the built-in LED a few times
static esp_err_t blink_get_handler(httpd_req_t *req)
{
    // Blink the built-in LED 3 times
    for (int i = 0; i < 10; i++) {
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    httpd_resp_send(req, "Blinked", HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "Blink test passed.");

    return ESP_OK;
}

// URI handlers
static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t move = {
    .uri       = "/move",
    .method    = HTTP_GET,
    .handler   = move_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t blink = {
    .uri       = "/blink",
    .method    = HTTP_GET,
    .handler   = blink_get_handler,
    .user_ctx  = NULL
};

// Start web server
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting HTTP server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &move);
        httpd_register_uri_handler(server, &blink);
        return server;
    }

    Serial.print("Error starting HTTP server!");
    return NULL;
}

