// Excerpt from main.cpp (P4 Wall Keypad). Not buildable on its own.
//
// Keeping the keypad in sync with Home Assistant. On MQTT connect the keypad
// subscribes to entity state and asks HA to resend it. It also watches HA's
// birth message (homeassistant/status) and asks again whenever HA comes back
// online, so the tiles are correct even after an HA restart.

static void mqtt_event_handler(void *args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "MQTT connected");
            mqtt_connected = true;
            ui_sync_connectivity();
            // config.h subscriptions (skip slots owned by NVS web config)
            for (int i = 0; i < NUM_LIGHTS; i++) {
                if (nvs_buttons[i].configured) {
                    continue;
                }
                if (LIGHT_CONFIG[i].state_topic) {
                    esp_mqtt_client_subscribe(mqtt_client, LIGHT_CONFIG[i].state_topic, 1);
                }
            }
            // HA state sync — one wildcard covers all entities
            esp_mqtt_client_subscribe(mqtt_client, "keypad/state/#", 1);
            ESP_LOGI(TAG, "Subscribed: keypad/state/#");

            // Listen for HA's birth message. HA publishes "online" to
            // homeassistant/status (retained) when it finishes booting. We
            // re-request resync on every "online" so the Resync automation
            // is guaranteed to be subscribed by the time the request fires.
            esp_mqtt_client_subscribe(mqtt_client, "homeassistant/status", 1);
            ESP_LOGI(TAG, "Subscribed: homeassistant/status");

            // Also fire one resync request immediately — covers installs where
            // homeassistant/status isn't configured. Retained automation can
            // ignore the duplicate.
            esp_mqtt_client_publish(mqtt_client, "keypad/sync/request", "1", 1, 1, 0);
            ESP_LOGI(TAG, "Published keypad/sync/request (on connect)");
            break;
        }

        case MQTT_EVENT_DATA: {
            char topic[128];
            int len = event->topic_len < 127 ? event->topic_len : 127;
            strncpy(topic, event->topic, len); topic[len] = '\0';

            char data[64];
            int dlen = event->data_len < 63 ? event->data_len : 63;
            strncpy(data, event->data, dlen); data[dlen] = '\0';

            // HA birth/will. On "online" — either because HA just finished
            // booting or because the broker delivered the retained value at
            // subscribe time — push a fresh sync request. Resync automation
            // is guaranteed loaded by the time HA reports online.
            if (strcmp(topic, "homeassistant/status") == 0) {
                ESP_LOGI(TAG, "HA status: %s", data);
                if (strcasecmp(data, "online") == 0) {
                    esp_mqtt_client_publish(mqtt_client, "keypad/sync/request", "1", 1, 1, 0);
                    ESP_LOGI(TAG, "Published keypad/sync/request (HA online)");
                }
                break;
            }
            // ... (per-entity state handling continues)
