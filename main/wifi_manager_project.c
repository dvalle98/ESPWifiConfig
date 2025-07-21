#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

#define WIFI_SSID_DEFAULT "ESP32_Config"
#define WIFI_PASSWORD_DEFAULT "123456789"
#define MAX_APS 20
#define WIFI_SCAN_TIMEOUT 10000 // 10 segundos
#define WIFI_CONNECT_TIMEOUT 15000 // 15 segundos

static const char *TAG = "wifi_manager";
static httpd_handle_t server = NULL;
static bool is_connected = false;


// Definición de pines GPIO para periféricos
#define GPIO_RELAY GPIO_NUM_4   // Relé
#define GPIO_BUZZER GPIO_NUM_15 // Buzzer
#define GPIO_BTN_AP GPIO_NUM_25 // Botón (para entrada)
#define WIFI_LED GPIO_NUM_32

// Inicializa los GPIOs como salidas y los pone en bajo
static void gpio_init(void)
{
    // Reset pines antes de configurarlos
    gpio_reset_pin(GPIO_RELAY);
    gpio_reset_pin(GPIO_BUZZER);
    gpio_reset_pin(GPIO_BTN_AP);
    gpio_reset_pin(WIFI_LED);

    // Configurar salidas (LEDs, relé, buzzer)
    gpio_config_t io_conf_output = {
        (1ULL << GPIO_RELAY) |
            (1ULL << GPIO_BUZZER) | (1ULL << WIFI_LED), // Pines de salida
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf_output);

    // Apagar todos los periféricos al inicio
    gpio_set_level(WIFI_LED, 0); // LED Wi-Fi
    gpio_set_level(GPIO_RELAY, 0);
    gpio_set_level(GPIO_BUZZER, 0);

    // Configurar entrada con pull-up para el botón
    gpio_config_t io_conf_input = {
        .pin_bit_mask = (1ULL << GPIO_BTN_AP),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf_input);
}


static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi desconectado, intentando reconectar...");
        if (!is_connected) {
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        char ip_str[16];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));
        
        ESP_LOGI(TAG, "Conectado, IP: %s", ip_str);
        is_connected = true;
    }
}

static esp_err_t get_handler(httpd_req_t *req) {
    const char* html = "<!DOCTYPE html><html><body>"
                      "<h1>Configurar WiFi</h1>"
                      "<form method='post' action='/connect'>"
                      "<label>SSID:</label><input type='text' name='ssid'><br>"
                      "<label>Contraseña:</label><input type='password' name='password'><br>"
                      "<input type='submit' value='Conectar'>"
                      "</form></body></html>";
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

static esp_err_t post_handler(httpd_req_t *req) {
    char buf[100];
    int ret, len = httpd_req_recv(req, buf, sizeof(buf));
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';

    char ssid[32] = {0};
    char password[64] = {0};
    // Parsear SSID y contraseña desde el formulario (simplificado)
    sscanf(buf, "ssid=%32[^&]&password=%64s", ssid, password);

    nvs_handle_t nvs_handle;
    nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
    nvs_set_str(nvs_handle, "ssid", ssid);
    nvs_set_str(nvs_handle, "password", password);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = ""
        }
    };
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_connect();

    const char* resp = "Conectando... <a href='/'>Volver</a>";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server, &config);

    httpd_uri_t get_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &get_uri);

    httpd_uri_t post_uri = {
        .uri = "/connect",
        .method = HTTP_POST,
        .handler = post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &post_uri);
}

static void wifi_manager_start(void) {
    // Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicializar red
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    // Configurar WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // Intentar conectar con credenciales guardadas
    nvs_handle_t nvs_handle;
    nvs_open("wifi_config", NVS_READONLY, &nvs_handle);
    char ssid[32] = {0};
    char password[64] = {0};
    size_t len = sizeof(ssid);
    ret = nvs_get_str(nvs_handle, "ssid", ssid, &len);
    if (ret == ESP_OK) {
        len = sizeof(password);
        nvs_get_str(nvs_handle, "password", password, &len);
        wifi_config_t wifi_config = {
            .sta = {
                .ssid = "",
                .password = ""
            }
        };
        strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_connect();
    } else {
        // No hay credenciales, iniciar AP
        wifi_config_t ap_config = {
            .ap = {
                .ssid = WIFI_SSID_DEFAULT,
                .ssid_len = strlen(WIFI_SSID_DEFAULT),
                .password = WIFI_PASSWORD_DEFAULT,
                .max_connection = 4,
                .authmode = WIFI_AUTH_WPA2_PSK
            }
        };
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        start_webserver();
    }
    nvs_close(nvs_handle);
}

void app_main(void) {

    ESP_LOGI(TAG, "Iniciando aplicación MQTT Control...");
    gpio_init();      // Inicializa GPIOs

    wifi_manager_start();
}