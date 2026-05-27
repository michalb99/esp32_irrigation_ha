#include "IrrigationMqtt.h"

#include <cstdio>
#include <cstring>
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "IrrigationMqtt";

// ─── Construction / destruction ──────────────────────────────────────────────

IrrigationMqtt::IrrigationMqtt(const gpio_num_t *relay_gpios,
                                 const char *broker_host, uint16_t broker_port,
                                 const char *node_id,
                                 const char *username, const char *password)
    : m_broker_host(broker_host), m_broker_port(broker_port),
      m_node_id(node_id),
      m_username(username ? username : ""),
      m_password(password ? password : ""),
      m_client(nullptr), m_connected(false),
      m_relay_queue(nullptr), m_last_on_tick(0)
{
    for (int i = 0; i < RELAY_COUNT; i++) {
        m_gpios[i]  = relay_gpios[i];
        m_states[i] = false;
    }
}

IrrigationMqtt::~IrrigationMqtt()
{
    if (m_client) {
        esp_mqtt_client_stop(m_client);
        esp_mqtt_client_destroy(m_client);
    }
}

// ─── Public API ──────────────────────────────────────────────────────────────

esp_err_t IrrigationMqtt::begin()
{
    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.hostname  = m_broker_host.c_str();
    cfg.broker.address.port      = m_broker_port;
    cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    cfg.credentials.client_id    = m_node_id.c_str();
    if (!m_username.empty()) {
        cfg.credentials.username = m_username.c_str();
        cfg.credentials.authentication.password = m_password.c_str();
    }

    m_client = esp_mqtt_client_init(&cfg);
    if (!m_client) {
        ESP_LOGE(TAG, "Failed to initialise MQTT client");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        m_client,
        static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
        mqttEventHandler, this));

    // Queue holds up to RELAY_COUNT * 2 pending commands.
    m_relay_queue  = xQueueCreate(RELAY_COUNT * 2, sizeof(RelayCommand));
    // Allow the first activation immediately (no artificial boot delay).
    m_last_on_tick = xTaskGetTickCount() - pdMS_TO_TICKS(RELAY_ON_MIN_GAP_MS);
    xTaskCreate(relayTaskEntry, "relay_ctrl", 4096, this, 5, nullptr);

    return esp_mqtt_client_start(m_client);
}

// ─── Private ─────────────────────────────────────────────────────────────────

void IrrigationMqtt::mqttEventHandler(void *handler_args, esp_event_base_t base,
                                       int32_t event_id, void *event_data)
{
    auto *self = static_cast<IrrigationMqtt *>(handler_args);

    switch (static_cast<esp_mqtt_event_id_t>(event_id)) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to broker");
            self->m_connected = true;
            self->onConnected();
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected from broker");
            self->m_connected = false;
            break;

        case MQTT_EVENT_DATA: {
            auto *e = static_cast<esp_mqtt_event_handle_t>(event_data);
            self->onData(e->topic, e->topic_len, e->data, e->data_len);
            break;
        }

        case MQTT_EVENT_ERROR: {
            auto *event = static_cast<esp_mqtt_event_handle_t>(event_data);
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "TCP transport error 0x%x",
                         event->error_handle->esp_tls_last_esp_err);
            }
            break;
        }

        default:
            break;
    }
}

void IrrigationMqtt::onConnected()
{
    publishDeviceDiscovery();

    for (int i = 0; i < RELAY_COUNT; i++) {
        char cmd_topic[128];
        snprintf(cmd_topic, sizeof(cmd_topic),
                 "homeassistant/switch/%s_relay%d/set",
                 m_node_id.c_str(), i + 1);
        esp_mqtt_client_subscribe(m_client, cmd_topic, /*qos*/1);
        ESP_LOGI(TAG, "Subscribed to %s", cmd_topic);

        publishState(i);
    }
}

void IrrigationMqtt::onData(const char *topic, int topic_len,
                              const char *data,  int data_len)
{
    for (int i = 0; i < RELAY_COUNT; i++) {
        char cmd_topic[128];
        int  len = snprintf(cmd_topic, sizeof(cmd_topic),
                            "homeassistant/switch/%s_relay%d/set",
                            m_node_id.c_str(), i + 1);

        if (topic_len == len && strncmp(topic, cmd_topic, (size_t)topic_len) == 0) {
            bool on = (data_len >= 2 && strncasecmp(data, "ON", 2) == 0);
            ESP_LOGI(TAG, "Relay %d command -> %s", i + 1, on ? "ON" : "OFF");
            RelayCommand cmd = { i, on };
            if (xQueueSend(m_relay_queue, &cmd, 0) != pdTRUE) {
                ESP_LOGW(TAG, "Relay command queue full, dropping command");
            }
            return;
        }
    }
}

// ─── Relay task ──────────────────────────────────────────────────────────────

void IrrigationMqtt::relayTaskEntry(void *arg)
{
    static_cast<IrrigationMqtt *>(arg)->relayTask();
}

void IrrigationMqtt::relayTask()
{
    while (true) {
        RelayCommand cmd;
        xQueueReceive(m_relay_queue, &cmd, portMAX_DELAY);

        if (cmd.on) {
            // Enforce minimum gap between successive ON activations.
            TickType_t elapsed = xTaskGetTickCount() - m_last_on_tick;
            TickType_t gap     = pdMS_TO_TICKS(RELAY_ON_MIN_GAP_MS);
            if (elapsed < gap) {
                TickType_t wait = gap - elapsed;
                ESP_LOGI(TAG, "Relay %d: waiting %lu ms before ON",
                         cmd.relay_idx + 1, (unsigned long)wait * portTICK_PERIOD_MS);
                vTaskDelay(wait);
            }
            setRelay(cmd.relay_idx, true);
            m_last_on_tick = xTaskGetTickCount();
        } else {
            setRelay(cmd.relay_idx, false);
        }

        publishState(cmd.relay_idx);
    }
}

void IrrigationMqtt::setRelay(int relay_idx, bool on)
{
    m_states[relay_idx] = on;
    // Active-LOW relay module: GPIO LOW energises the relay.
    gpio_set_level(m_gpios[relay_idx], on ? 0 : 1);
}

void IrrigationMqtt::publishState(int relay_idx)
{
    if (!m_connected || !m_client) return;

    char state_topic[128];
    snprintf(state_topic, sizeof(state_topic),
             "homeassistant/switch/%s_relay%d/state",
             m_node_id.c_str(), relay_idx + 1);

    const char *payload = m_states[relay_idx] ? "ON" : "OFF";
    esp_mqtt_client_publish(m_client, state_topic, payload, 0, /*qos*/1, /*retain*/1);
    ESP_LOGD(TAG, "State relay %d -> %s", relay_idx + 1, payload);
}

void IrrigationMqtt::publishDeviceDiscovery()
{
    // Build one component entry per relay.
    char cmps[RELAY_COUNT][384];
    for (int i = 0; i < RELAY_COUNT; i++) {
        snprintf(cmps[i], sizeof(cmps[i]),
                 "\"relay%d\":{"
                   "\"p\":\"switch\","
                   "\"name\":\"Irrigation Zone %d\","
                   "\"unique_id\":\"%s_relay%d\","
                   "\"state_topic\":\"homeassistant/switch/%s_relay%d/state\","
                   "\"command_topic\":\"homeassistant/switch/%s_relay%d/set\","
                   "\"payload_on\":\"ON\","
                   "\"payload_off\":\"OFF\","
                   "\"state_on\":\"ON\","
                   "\"state_off\":\"OFF\","
                   "\"retain\":true,"
                   "\"device_class\":\"switch\""
                 "}",
                 i + 1, i + 1,
                 m_node_id.c_str(), i + 1,
                 m_node_id.c_str(), i + 1,
                 m_node_id.c_str(), i + 1);
    }

    char config_topic[128];
    snprintf(config_topic, sizeof(config_topic),
             "homeassistant/device/%s/config", m_node_id.c_str());

    char payload[1536];
    snprintf(payload, sizeof(payload),
             "{"
               "\"device\":{"
                 "\"identifiers\":[\"%s\"],"
                 "\"name\":\"Irrigation Controller\","
                 "\"model\":\"ESP32-C3\","
                 "\"manufacturer\":\"Espressif\""
               "},"
               "\"origin\":{"
                 "\"name\":\"%s\""
               "},"
               "\"components\":{"
                 "%s,%s,%s"
               "}"
             "}",
             m_node_id.c_str(),
             m_node_id.c_str(),
             cmps[0], cmps[1], cmps[2]);

    esp_mqtt_client_publish(m_client, config_topic, payload, 0, /*qos*/1, /*retain*/1);
    ESP_LOGI(TAG, "Device discovery published to %s", config_topic);
}
