#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_mac.h"

#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

// ======= SWITCH HERE =======
#define USE_11B_ONLY   1
// ===========================

#define AP_SSID        (USE_11B_ONLY ? "ESP32C6_AP_11b" : "ESP32C6_AP")
#define AP_PASSWORD    "mypassword"   
#define AP_CHANNEL     6                 
#define AP_MAX_CONN    8
#define AP_AUTHMODE    WIFI_AUTH_WPA2_PSK
#define AP_BEACON_MS   100

#define UDP_PORT       5001
#define UDP_BUF_LEN    2048

static const char *TAG = "softap";
static esp_netif_t *ap_netif = NULL;
static TaskHandle_t udp_task_hdl = NULL;

static void udp_echo_task(void *arg)
{
    esp_netif_ip_info_t ipinfo = {0};
    if (esp_netif_get_ip_info(ap_netif, &ipinfo) != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_get_ip_info failed");
        vTaskDelete(NULL);
        return;
    }
    char ip_str[16];
    ip4addr_ntoa_r(&ipinfo.ip, ip_str, sizeof(ip_str));

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() failed");
        vTaskDelete(NULL);
        return;
    }

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));

    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(UDP_PORT);
    addr.sin_addr.s_addr = inet_addr(ip_str);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind(%s:%d) failed", ip_str, UDP_PORT);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UDP echo listening on %s:%d (%s)", ip_str, UDP_PORT,
             USE_11B_ONLY ? "11b-only" : "b/g/n no-CCK");

    uint8_t *buf = (uint8_t *)malloc(UDP_BUF_LEN);
    if (!buf) {
        ESP_LOGE(TAG, "malloc failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        struct sockaddr_in from = {0};
        socklen_t flen = sizeof(from);
        int n = recvfrom(sock, buf, UDP_BUF_LEN, 0, (struct sockaddr *)&from, &flen);
        if (n < 0) continue;

        char from_ip[16];
        inet_ntop(AF_INET, &from.sin_addr, from_ip, sizeof(from_ip));
        uint16_t from_port = ntohs(from.sin_port);
        //ESP_LOGI(TAG, "RX %d bytes from %s:%u", n, from_ip, from_port);

        (void)sendto(sock, buf, n, 0, (struct sockaddr *)&from, flen);
    }
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "SoftAP started: SSID=%s, channel=%d, mode=%s",
                 AP_SSID, AP_CHANNEL, USE_11B_ONLY ? "11b-only" : "b/g/n (no CCK)");
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(ap_netif, &ip) == ESP_OK) {
            ESP_LOGI(TAG, "AP IP: " IPSTR ", GW: " IPSTR ", NM: " IPSTR,
                     IP2STR(&ip.ip), IP2STR(&ip.gw), IP2STR(&ip.netmask));
        }
        if (udp_task_hdl == NULL) {
            xTaskCreatePinnedToCore(udp_echo_task, "udp_echo", 4096, NULL, 5, &udp_task_hdl, tskNO_AFFINITY);
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *e = (const wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "STA " MACSTR " joined, AID=%d", MAC2STR(e->mac), e->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *e = (const wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "STA " MACSTR " left,  AID=%d", MAC2STR(e->mac), e->aid);
    }
}

static void start_softap(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);

    esp_netif_ip_info_t ip;
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
    IP4_ADDR(&ip.ip,      192, 168, 4, 1);
    IP4_ADDR(&ip.gw,      192, 168, 4, 1);
    IP4_ADDR(&ip.netmask, 255, 255, 255, 0);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t ap_cfg = { 0 };
    strncpy((char *)ap_cfg.ap.ssid, AP_SSID, sizeof(ap_cfg.ap.ssid));
    ap_cfg.ap.ssid_len        = strlen(AP_SSID);
    strncpy((char *)ap_cfg.ap.password, AP_PASSWORD, sizeof(ap_cfg.ap.password));
    ap_cfg.ap.channel         = AP_CHANNEL;
    ap_cfg.ap.max_connection  = AP_MAX_CONN;
    ap_cfg.ap.authmode        = (strlen(AP_PASSWORD) == 0) ? WIFI_AUTH_OPEN : AP_AUTHMODE;
    ap_cfg.ap.beacon_interval = AP_BEACON_MS;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));

    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20));

    if (USE_11B_ONLY) {
        // Pure 11b
        uint8_t proto = WIFI_PROTOCOL_11B;
        ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_AP, proto));
        // Ensure 11b data rates are ENABLED (false = don't disable)
        esp_err_t e = esp_wifi_config_11b_rate(WIFI_IF_AP, false);
        if (e == ESP_OK) {
            ESP_LOGI(TAG, "11b data rates ENABLED (pure 11b).");
        } else {
            ESP_LOGW(TAG, "esp_wifi_config_11b_rate not applied (err=0x%x).", (unsigned)e);
        }
    } else {
        uint8_t proto = WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N;
        ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_AP, proto));

        esp_err_t e = esp_wifi_config_11b_rate(WIFI_IF_AP, true);
        if (e == ESP_OK) {
            ESP_LOGI(TAG, "11b data rates DISABLED (no CCK fallback).");
        } else {
            ESP_LOGW(TAG, "esp_wifi_config_11b_rate not applied (err=0x%x).", (unsigned)e);
        }
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "AP up (%s). Send UDP to 192.168.4.1:%d",
             USE_11B_ONLY ? "11b-only" : "b/g/n no-CCK", UDP_PORT);
}

void app_main(void)
{
    start_softap();
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}