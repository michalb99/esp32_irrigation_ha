#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_event.h"

/**
 * @brief Manages WiFi STA connection with automatic reconnect.
 *
 * Typical usage:
 *   WifiManager wifi(WIFI_SSID, WIFI_PASS);
 *   ESP_ERROR_CHECK(wifi.connect());   // blocks until IP obtained
 */
class WifiManager {
public:
    WifiManager(const char *ssid, const char *password);

    /**
     * Initialise the TCP/IP stack, WiFi driver and event loop, then block
     * until an IP address is assigned.
     */
    esp_err_t connect();

private:
    static void eventHandler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data);

    const char        *m_ssid;
    const char        *m_password;
    EventGroupHandle_t m_event_group;
};
