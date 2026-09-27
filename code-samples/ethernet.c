#include "ethernet.h"

#include <string.h>
#include <stdio.h>

#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_check.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "lwip/ip4_addr.h"
#include "lwip/sockets.h"

#include "mdns.h"
#include "ping/ping_sock.h"

static const char *TAG = "ETH";

#define NVS_NS              "net_cfg"
#define DEFAULT_HOSTNAME    "keypad"

// Event group bits for synchronization between event handler and ethernet_init().
#define BIT_GOT_IP          BIT0
#define BIT_LINK_DOWN       BIT1

static EventGroupHandle_t s_eth_events = NULL;
static esp_netif_t       *s_eth_netif  = NULL;
static esp_eth_handle_t   s_eth_handle = NULL;

// Live state, updated by event handler. Protected by the fact that all
// reads happen from the webserver (single thread) and writes from the
// event loop task (single thread). For atomic snapshots we copy under
// a brief critical section.
static bool s_link_up           = false;
static bool s_got_ip            = false;
static bool s_fallback_active   = false;  // true if static failed and we fell back to DHCP
static esp_netif_ip_info_t s_ip_info = {0};
static char s_hostname[24]      = DEFAULT_HOSTNAME;

// =====================================================================
// NVS helpers
// =====================================================================
static void load_str(nvs_handle_t h, const char *key, char *out, size_t out_sz, const char *def) {
    size_t len = out_sz;
    out[0] = '\0';
    if (nvs_get_str(h, key, out, &len) != ESP_OK || out[0] == '\0') {
        strncpy(out, def, out_sz - 1);
        out[out_sz - 1] = '\0';
    }
}

bool net_config_load(net_config_t *out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    // Defaults
    out->mode = NET_MODE_DHCP;
    strcpy(out->hostname, DEFAULT_HOSTNAME);

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        return true; // no saved config; defaults are fine
    }
    uint8_t mode_u8 = 0;
    if (nvs_get_u8(h, "mode", &mode_u8) == ESP_OK) {
        out->mode = (mode_u8 == 1) ? NET_MODE_STATIC : NET_MODE_DHCP;
    }
    load_str(h, "ip",       out->ip,       sizeof(out->ip),       "");
    load_str(h, "mask",     out->netmask,  sizeof(out->netmask),  "");
    load_str(h, "gw",       out->gateway,  sizeof(out->gateway),  "");
    load_str(h, "dns1",     out->dns1,     sizeof(out->dns1),     "");
    load_str(h, "dns2",     out->dns2,     sizeof(out->dns2),     "");
    load_str(h, "hostname", out->hostname, sizeof(out->hostname), DEFAULT_HOSTNAME);
    nvs_close(h);
    return true;
}

bool net_config_save(const net_config_t *cfg) {
    if (!cfg) return false;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_set_u8(h, "mode", (uint8_t)cfg->mode);
    nvs_set_str(h, "ip",       cfg->ip);
    nvs_set_str(h, "mask",     cfg->netmask);
    nvs_set_str(h, "gw",       cfg->gateway);
    nvs_set_str(h, "dns1",     cfg->dns1);
    nvs_set_str(h, "dns2",     cfg->dns2);
    nvs_set_str(h, "hostname", cfg->hostname[0] ? cfg->hostname : DEFAULT_HOSTNAME);
    esp_err_t err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

// =====================================================================
// Event handlers
// =====================================================================
static void on_eth_event(void *arg, esp_event_base_t base, int32_t event_id, void *data) {
    (void)arg; (void)base; (void)data;
    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Link UP");
            s_link_up = true;
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Link DOWN");
            s_link_up = false;
            s_got_ip = false;
            if (s_eth_events) xEventGroupSetBits(s_eth_events, BIT_LINK_DOWN);
            break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet started");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet stopped");
            break;
        default:
            break;
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t event_id, void *data) {
    (void)arg; (void)base;
    if (event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        s_ip_info = e->ip_info;
        s_got_ip = true;
        ESP_LOGI(TAG, "Got IP: " IPSTR "  GW: " IPSTR "  Mask: " IPSTR,
                 IP2STR(&e->ip_info.ip),
                 IP2STR(&e->ip_info.gw),
                 IP2STR(&e->ip_info.netmask));
        if (s_eth_events) xEventGroupSetBits(s_eth_events, BIT_GOT_IP);
    } else if (event_id == IP_EVENT_ETH_LOST_IP) {
        ESP_LOGW(TAG, "Lost IP");
        s_got_ip = false;
    }
}

// =====================================================================
// Apply config (DHCP vs static)
// =====================================================================
static bool parse_ip4(const char *s, esp_ip4_addr_t *out) {
    if (!s || !s[0]) return false;
    ip4_addr_t tmp;
    if (ip4addr_aton(s, &tmp) == 0) return false;
    out->addr = tmp.addr;
    return true;
}

// Same as parse_ip4 but writes the raw 32-bit address. Used for filling
// lwIP-flavored structs (ip4_addr_t) which differ from esp_ip4_addr_t.
static bool parse_ip4_raw(const char *s, uint32_t *out_addr) {
    if (!s || !s[0]) return false;
    ip4_addr_t tmp;
    if (ip4addr_aton(s, &tmp) == 0) return false;
    *out_addr = tmp.addr;
    return true;
}

static esp_err_t apply_static_ip(const net_config_t *cfg) {
    esp_netif_ip_info_t ip_info = {0};
    if (!parse_ip4(cfg->ip,      &ip_info.ip))      { ESP_LOGE(TAG, "bad IP: %s",   cfg->ip);      return ESP_ERR_INVALID_ARG; }
    if (!parse_ip4(cfg->netmask, &ip_info.netmask)) { ESP_LOGE(TAG, "bad mask: %s", cfg->netmask); return ESP_ERR_INVALID_ARG; }
    if (!parse_ip4(cfg->gateway, &ip_info.gw))      { ESP_LOGE(TAG, "bad GW: %s",   cfg->gateway); return ESP_ERR_INVALID_ARG; }

    esp_netif_dhcpc_stop(s_eth_netif);  // ignore errors; may not be running
    ESP_RETURN_ON_ERROR(esp_netif_set_ip_info(s_eth_netif, &ip_info), TAG, "set_ip_info");

    // Set DNS if provided. If not, the gateway is usually a fine fallback.
    esp_netif_dns_info_t dns = {0};
    if (cfg->dns1[0]) {
        if (parse_ip4_raw(cfg->dns1, &dns.ip.u_addr.ip4.addr)) {
            dns.ip.type = ESP_IPADDR_TYPE_V4;
            esp_netif_set_dns_info(s_eth_netif, ESP_NETIF_DNS_MAIN, &dns);
        }
    } else {
        dns.ip.u_addr.ip4.addr = ip_info.gw.addr;
        dns.ip.type = ESP_IPADDR_TYPE_V4;
        esp_netif_set_dns_info(s_eth_netif, ESP_NETIF_DNS_MAIN, &dns);
    }
    if (cfg->dns2[0]) {
        memset(&dns, 0, sizeof(dns));
        if (parse_ip4_raw(cfg->dns2, &dns.ip.u_addr.ip4.addr)) {
            dns.ip.type = ESP_IPADDR_TYPE_V4;
            esp_netif_set_dns_info(s_eth_netif, ESP_NETIF_DNS_BACKUP, &dns);
        }
    }

    // Manually signal "got IP" since DHCP isn't running.
    s_ip_info = ip_info;
    s_got_ip = true;
    if (s_eth_events) xEventGroupSetBits(s_eth_events, BIT_GOT_IP);
    return ESP_OK;
}

static esp_err_t apply_dhcp(void) {
    esp_netif_ip_info_t blank = {0};
    esp_netif_set_ip_info(s_eth_netif, &blank);   // clear any stale static
    esp_err_t err = esp_netif_dhcpc_start(s_eth_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        ESP_LOGE(TAG, "dhcpc_start: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

// =====================================================================
// Gateway ping (validates static IP works)
// Returns true if at least one reply received within timeout.
// =====================================================================
static volatile int s_ping_replies = 0;

static void on_ping_success(esp_ping_handle_t hdl, void *args) {
    (void)hdl; (void)args;
    s_ping_replies++;
}

static bool ping_gateway(const esp_ip4_addr_t *gw_ip, int count, int timeout_ms) {
    s_ping_replies = 0;

    esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
    cfg.target_addr.u_addr.ip4.addr = gw_ip->addr;
    cfg.target_addr.type = ESP_IPADDR_TYPE_V4;
    cfg.count   = count;
    cfg.interval_ms = 1000;
    cfg.timeout_ms  = 1000;

    esp_ping_callbacks_t cbs = {
        .on_ping_success = on_ping_success,
        .on_ping_timeout = NULL,
        .on_ping_end     = NULL,
        .cb_args         = NULL,
    };
    esp_ping_handle_t hdl;
    if (esp_ping_new_session(&cfg, &cbs, &hdl) != ESP_OK) return false;
    esp_ping_start(hdl);

    // Wait for the session to complete, bounded by timeout_ms.
    int waited = 0;
    while (waited < timeout_ms && s_ping_replies == 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        waited += 100;
    }
    // Give it a small tail in case a reply was in flight
    if (s_ping_replies == 0) vTaskDelay(pdMS_TO_TICKS(500));

    esp_ping_stop(hdl);
    esp_ping_delete_session(hdl);
    return s_ping_replies > 0;
}

// =====================================================================
// mDNS
// =====================================================================
static void mdns_setup(const char *hostname) {
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mdns_init failed: %s", esp_err_to_name(err));
        return;
    }
    mdns_hostname_set(hostname);
    mdns_instance_name_set("Keypad V2");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS up: http://%s.local", hostname);
}

// =====================================================================
// Public init
// =====================================================================
void ethernet_init(void) {
    // The webserver code already calls nvs_flash_init(); main.cpp also does.
    // Calling it again is a safe no-op.
    esp_err_t nerr = nvs_flash_init();
    if (nerr == ESP_ERR_NVS_NO_FREE_PAGES || nerr == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    net_config_t cfg;
    net_config_load(&cfg);
    strncpy(s_hostname, cfg.hostname, sizeof(s_hostname) - 1);
    s_hostname[sizeof(s_hostname) - 1] = '\0';

    ESP_ERROR_CHECK(esp_netif_init());
    // esp_event_loop_create_default() — safe to call once; ignore "already created"
    esp_err_t lerr = esp_event_loop_create_default();
    if (lerr != ESP_OK && lerr != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(lerr);
    }

    s_eth_events = xEventGroupCreate();

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_cfg);

    eth_esp32_emac_config_t emac_cfg = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_cfg, &mac_cfg);
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_cfg);

    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_cfg, &s_eth_handle));
    ESP_ERROR_CHECK(esp_netif_attach(s_eth_netif, esp_eth_new_netif_glue(s_eth_handle)));

    // Register handlers BEFORE starting the driver so we don't miss early events
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, on_eth_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,  IP_EVENT_ETH_GOT_IP, on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,  IP_EVENT_ETH_LOST_IP, on_ip_event, NULL));

    // Set the kernel-level hostname (used in DHCP requests too)
    esp_netif_set_hostname(s_eth_netif, s_hostname);

    // If we're configured for static IP, stop the auto DHCP client BEFORE start.
    if (cfg.mode == NET_MODE_STATIC) {
        esp_netif_dhcpc_stop(s_eth_netif);
    }

    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));

    // Wait for link up (best effort, up to 5s)
    ESP_LOGI(TAG, "Waiting for link...");
    int waited = 0;
    while (!s_link_up && waited < 5000) {
        vTaskDelay(pdMS_TO_TICKS(100));
        waited += 100;
    }
    if (!s_link_up) {
        ESP_LOGW(TAG, "No link after 5s; continuing anyway (cable might be unplugged)");
    }

    // Apply the configured mode
    if (cfg.mode == NET_MODE_STATIC) {
        ESP_LOGI(TAG, "Applying static IP: %s/%s gw=%s", cfg.ip, cfg.netmask, cfg.gateway);
        if (apply_static_ip(&cfg) == ESP_OK) {
            // Validate by pinging the gateway
            esp_ip4_addr_t gw;
            if (parse_ip4(cfg.gateway, &gw)) {
                ESP_LOGI(TAG, "Validating static IP by pinging gateway (10s)...");
                bool ok = ping_gateway(&gw, 5, 10000);
                if (!ok) {
                    ESP_LOGW(TAG, "Gateway unreachable -- falling back to DHCP");
                    s_fallback_active = true;
                    s_got_ip = false;
                    apply_dhcp();
                } else {
                    ESP_LOGI(TAG, "Gateway reachable -- static IP confirmed");
                }
            }
        } else {
            ESP_LOGW(TAG, "Static config invalid -- falling back to DHCP");
            s_fallback_active = true;
            apply_dhcp();
        }
    } else {
        ESP_LOGI(TAG, "Using DHCP");
        apply_dhcp();
    }

    // Wait until we actually have an IP (event-driven for DHCP, instant for static-OK)
    ESP_LOGI(TAG, "Waiting for IP...");
    EventBits_t bits = xEventGroupWaitBits(s_eth_events, BIT_GOT_IP,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));
    if (!(bits & BIT_GOT_IP)) {
        ESP_LOGW(TAG, "No IP after 15s. Network may be unreachable.");
    }

    // Bring up mDNS regardless -- as long as we're on a link it announces.
    mdns_setup(s_hostname);
}

// =====================================================================
// Status query for web UI
// =====================================================================
bool net_get_status(char *ip_out, char *mask_out, char *gw_out,
                    char *dns_out, char *hostname_out,
                    bool *is_static_out, bool *fallback_active_out,
                    bool *link_up_out) {
    if (ip_out)   snprintf(ip_out,   16, IPSTR, IP2STR(&s_ip_info.ip));
    if (mask_out) snprintf(mask_out, 16, IPSTR, IP2STR(&s_ip_info.netmask));
    if (gw_out)   snprintf(gw_out,   16, IPSTR, IP2STR(&s_ip_info.gw));

    if (dns_out) {
        esp_netif_dns_info_t dns = {0};
        if (s_eth_netif && esp_netif_get_dns_info(s_eth_netif, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK) {
            snprintf(dns_out, 16, IPSTR, IP2STR(&dns.ip.u_addr.ip4));
        } else {
            dns_out[0] = '\0';
        }
    }

    if (hostname_out) {
        strncpy(hostname_out, s_hostname, 23);
        hostname_out[23] = '\0';
    }

    if (is_static_out || fallback_active_out) {
        net_config_t cfg;
        net_config_load(&cfg);
        if (is_static_out)       *is_static_out = (cfg.mode == NET_MODE_STATIC);
        if (fallback_active_out) *fallback_active_out = s_fallback_active;
    }
    if (link_up_out) *link_up_out = s_link_up;
    return s_got_ip;
}