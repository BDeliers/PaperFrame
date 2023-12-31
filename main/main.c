/*
 *  PaperFrame
 *  Started from captive_portal example from Espressif
 */

#include <sys/param.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_sleep.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_http_server.h"
#include "dns_server.h"

#include "display_manager.h"

// index.html, script.js and style.css binary sources
extern const char html_start[] asm("_binary_index_html_start");
extern const char html_end[] asm("_binary_index_html_end");

extern const char script_start[] asm("_binary_script_js_start");
extern const char script_end[] asm("_binary_script_js_end");

extern const char style_start[] asm("_binary_style_css_start");
extern const char style_end[] asm("_binary_style_css_end");


static esp_err_t common_get_handler(httpd_req_t *req);
static esp_err_t buffer_post_handler(httpd_req_t *req);
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void wifi_init_softap(void);
static httpd_handle_t start_webserver(void);
static void goto_power_saving(void);

// GET uri for all pages
static const httpd_uri_t common_get_uri = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = common_get_handler,
    .user_ctx  = NULL
};

// POST uri for data upload
static const httpd_uri_t buffer_post_uri = {
    .uri       = "/upload",
    .method    = HTTP_POST,
    .handler   = buffer_post_handler,
    .user_ctx  = NULL
};

static const char*          TAG                         = "main";
static const char*          WIFI_SSID                   = "PaperFrame";
static bool                 image_received              = false;
static volatile  uint32_t   timeout_start               = 0;        // Startup time, used to make a timeout

// Handler for WiFi events 
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);

        // Reset power-down timeout to give the user two minutes
        timeout_start = xTaskGetTickCount();
    } 
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);

        // Stop the system when the user disconnects
        goto_power_saving();
    }
}

// Initialize access point
static void wifi_init_softap(void)
{
    // Configure network settings and security
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    // Make the AP config
    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len       = strlen(WIFI_SSID),
            .max_connection = 1,
            .authmode       = WIFI_AUTH_OPEN
        },
    };
    strcpy((char*) &wifi_config.ap.ssid, WIFI_SSID);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));

    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s', no password", WIFI_SSID);
}

// HTTP GET Handler for allpages to redirect to index.html
static esp_err_t common_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET requested: %s", req->uri);

    uint32_t data_len = 0;
    char* data_start  = 0;

    if (strcmp(req->uri, "/script.js") == 0)
    {
        data_start = script_start;
        data_len   = script_end - script_start;
        httpd_resp_set_type(req, "text/javascript");
    }
    else if (strcmp(req->uri, "/style.css") == 0)
    {
        data_start = style_start;
        data_len   = style_end - style_start;
        httpd_resp_set_type(req, "text/css");
    }
    else
    {
        data_start = html_start;
        data_len   = html_end - html_start;
        httpd_resp_set_type(req, "text/html");
    }

    httpd_resp_send(req, data_start, data_len);

    return ESP_OK;
}

// HTTP buffer POST upload handler
static esp_err_t buffer_post_handler(httpd_req_t *req)
{
    int ret            = ESP_FAIL;
    int remaining      = req->content_len;
    uint32_t idx       = 0;
    uint8_t* buff      = display_manager_get_framebuffer();
    uint32_t buff_size = display_manager_get_framebuffer_size();

    // We can't receive more than our buffer
    if (remaining > buff_size)
    {
        return ESP_FAIL;
    }

    // While we have incoming bytes
    while (remaining > 0)
    {
        // Read the data for the request
        if ((ret = httpd_req_recv(req, (char*) buff+idx, MIN(remaining, buff_size))) <= 0) 
        {
            // Retry in case of timeout
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) 
            {
                continue;
            }
            return ESP_FAIL;
        }

        remaining -= ret;
        idx       += ret;

        ESP_LOGI(TAG, "Received %d bytes", ret);
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);

    image_received = true;

    return ESP_OK;
}

// Start the web server
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server   = NULL;
    httpd_config_t config   = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;
    config.uri_match_fn     = httpd_uri_match_wildcard;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        ESP_LOGI(TAG, "Registering URI handlers");
        // Set URI handlers
        httpd_register_uri_handler(server, &common_get_uri);
        httpd_register_uri_handler(server, &buffer_post_uri);
    }
    return server;
}

static void goto_power_saving(void)
{
    // Display to lowest power consumption
    display_manager_power_saving();

    // Enable deep sleep for all RTC power domains
    /*for (uint8_t domain = 0; domain < ESP_PD_DOMAIN_MAX; domain++)
    {
        ESP_ERROR_CHECK(esp_sleep_pd_config(domain, ESP_PD_OPTION_OFF));
    }*/

    ESP_LOGI(TAG, "Going to deep sleep");

    // Power-off wifi
    ESP_ERROR_CHECK(esp_wifi_stop());

    vTaskDelay(1);

    // Force deep sleep now. No wakeup configured -> wakeup with reset
    esp_deep_sleep_start();
}

void app_main(void)
{
    /*
        Turn of warnings from HTTP server as redirecting traffic will yield
        lots of invalid requests
    */
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);


    // Initialize networking stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop needed by the  main app
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize NVS needed by Wi-Fi
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize Wi-Fi including netif with default config
    esp_netif_create_default_wifi_ap();

    // Initialise ESP32 in SoftAP mode
    wifi_init_softap();

    // Start the server for the first time
    start_webserver();

    // Start the DNS server that will redirect all queries to the softAP IP
    start_dns_server();

    // Just initialize the display driver
    if (!display_manager_init())
    {
        ESP_LOGE(TAG, "Failed to initialize display");
    }

    uint32_t timeout_in_ticks = 120*configTICK_RATE_HZ;  // One minute in terms of ticks
    timeout_start = xTaskGetTickCount();

    ESP_LOGI(TAG, "Starting main loop");

    while (1)
    {
        // We received a buffer
        if (image_received)
        {
            // Save it to NVS
            if(display_manager_save_framebuffer())
            {
                ESP_LOGI(TAG, "Framebuffer saved");
            }
            else
            {
                ESP_LOGW(TAG, "Failed to store framebuffer");
            }

            // Show it on the display
            if (!display_manager_show())
            {
                ESP_LOGE(TAG, "Failed to set display");
            }

            image_received = false;

            // Send the device and the display to deep sleep
            //goto_power_saving();
        }

        // Timeout expired
        if ((xTaskGetTickCount() - timeout_start) >= timeout_in_ticks)
        {
            goto_power_saving();
        }

        vTaskDelay(1);
    }
}
