#include "WifiManager.h"

#include <cstring>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

static const char *TAG            = "WifiManager";
static const int   WIFI_CONNECTED = BIT0;

// ─── Construction ─────────────────────────────────────────────────────────────

WifiManager::WifiManager(const char *ssid, const char *password)
    : m_ssid(ssid), m_password(password), m_event_group(nullptr)
{
}

// ─── Public API ──────────────────────────────────────────────────────────────

esp_err_t WifiManager::connect()
{
    m_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,  eventHandler, this, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,   IP_EVENT_STA_GOT_IP, eventHandler, this, &instance_got_ip));

    wifi_config_t wifi_cfg = {};
    strncpy(reinterpret_cast<char *>(wifi_cfg.sta.ssid),
            m_ssid,     sizeof(wifi_cfg.sta.ssid)     - 1);
    strncpy(reinterpret_cast<char *>(wifi_cfg.sta.password),
            m_password, sizeof(wifi_cfg.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to '%s'...", m_ssid);
    xEventGroupWaitBits(m_event_group, WIFI_CONNECTED,
                        pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "WiFi connected");
    return ESP_OK;
}

// ─── Private ─────────────────────────────────────────────────────────────────

void WifiManager::eventHandler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    auto *self = static_cast<WifiManager *>(arg);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected, reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        auto *e = static_cast<ip_event_got_ip_t *>(event_data);
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(self->m_event_group, WIFI_CONNECTED);
    }
}
