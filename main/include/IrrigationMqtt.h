#pragma once

#include <atomic>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "mqtt_client.h"
#include "driver/gpio.h"

#define RELAY_COUNT 3

/**
 * @brief Controls three irrigation relays via Home Assistant MQTT Discovery.
 *
 * On every broker connection:
 *   - Publishes HA discovery config for each relay (switch entity).
 *   - Subscribes to each relay's command topic.
 *   - Publishes the current (retained) state of each relay.
 *
 * Relay wiring convention: active-LOW module (GPIO LOW = relay energised).
 *
 * Topics (N = 1..3):
 *   Command : homeassistant/switch/<node_id>_relay<N>/set    (payload: ON | OFF)
 *   State   : homeassistant/switch/<node_id>_relay<N>/state  (payload: ON | OFF)
 *   Config  : homeassistant/switch/<node_id>_relay<N>/config (retained discovery)
 */
class IrrigationMqtt {
public:
    IrrigationMqtt(const gpio_num_t *relay_gpios,
                   const char *broker_host,
                   uint16_t    broker_port,
                   const char *node_id,
                   const char *username = nullptr,
                   const char *password = nullptr);
    ~IrrigationMqtt();

    /** Connect to MQTT broker (non-blocking). Call after WiFi is up. */
    esp_err_t begin();

    bool isConnected() const { return m_connected; }

private:
    struct RelayCommand {
        int  relay_idx;
        bool on;
    };

    static void mqttEventHandler(void *handler_args, esp_event_base_t base,
                                  int32_t event_id, void *event_data);
    void onConnected();
    void onData(const char *topic, int topic_len,
                const char *data,  int data_len);
    void publishDeviceDiscovery();
    void publishState(int relay_idx);
    void setRelay(int relay_idx, bool on);

    static void relayTaskEntry(void *arg);
    void        relayTask();

    gpio_num_t         m_gpios[RELAY_COUNT];
    std::atomic<bool> m_states[RELAY_COUNT];

    std::string               m_broker_host;
    uint16_t                  m_broker_port;
    std::string               m_node_id;
    std::string               m_username;
    std::string               m_password;
    esp_mqtt_client_handle_t  m_client;
    std::atomic<bool>         m_connected;

    QueueHandle_t m_relay_queue;
    TickType_t    m_last_on_tick;
};
