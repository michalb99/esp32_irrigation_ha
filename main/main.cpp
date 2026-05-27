#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "WifiManager.h"
#include "IrrigationMqtt.h"
#include "config.h"
#include "secrets.h"

static const gpio_num_t RELAY_GPIOS[RELAY_COUNT] = {
    static_cast<gpio_num_t>(RELAY_GPIO_1),
    static_cast<gpio_num_t>(RELAY_GPIO_2),
    static_cast<gpio_num_t>(RELAY_GPIO_3),
};

static WifiManager    wifi(WIFI_SSID, WIFI_PASS);
static IrrigationMqtt mqtt(RELAY_GPIOS, MQTT_BROKER_HOST, MQTT_BROKER_PORT,
                            MQTT_NODE_ID, MQTT_USERNAME, MQTT_PASSWORD);

extern "C" void app_main(void)
{
    // Initialise NVS (required by WiFi).
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Pre-set output latches HIGH (relay OFF) before enabling GPIO output.
    for (int i = 0; i < RELAY_COUNT; i++) {
        gpio_set_level(RELAY_GPIOS[i], 1);
    }

    // Configure all relay GPIOs as push-pull outputs with pull-ups.
    uint64_t pin_mask = 0;
    for (int i = 0; i < RELAY_COUNT; i++) {
        pin_mask |= (1ULL << RELAY_GPIOS[i]);
    }
    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    ESP_ERROR_CHECK(wifi.connect());
    // Brief settle time: lwIP needs a moment after getting an IP to join the
    // mDNS multicast group (224.0.0.251) before the MQTT client resolves
    // homeassistant.local via LWIP_DNS_SUPPORT_MDNS_QUERIES.
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_ERROR_CHECK(mqtt.begin());
    // MQTT event loop runs in its own task; app_main is no longer needed.
}
