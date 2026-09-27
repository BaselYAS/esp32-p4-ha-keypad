#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Network configuration mode persisted in NVS namespace "net_cfg".
typedef enum {
    NET_MODE_DHCP   = 0,
    NET_MODE_STATIC = 1,
} net_mode_t;

// Holds the user's preferred network configuration.
// IPs are stored as dotted-decimal strings (e.g. "192.168.1.50") in NVS,
// converted to uint32 on the fly when applied.
typedef struct {
    net_mode_t mode;
    char ip[16];        // e.g. "192.168.1.50"
    char netmask[16];   // e.g. "255.255.255.0"
    char gateway[16];   // e.g. "192.168.1.1"
    char dns1[16];      // e.g. "1.1.1.1" (optional, may be empty)
    char dns2[16];      // optional, may be empty
    char hostname[24];  // mDNS hostname, default "keypad"
} net_config_t;

// Initialize Ethernet, mDNS, and apply the saved net_config.
// Blocks until an IP is acquired or a 15s timeout elapses.
// Falls back to DHCP automatically if static IP can't reach gateway.
void ethernet_init(void);

// Read the current saved config from NVS into out. Returns true on success.
// If nothing is saved, fills out with defaults (DHCP mode, hostname "keypad").
bool net_config_load(net_config_t *out);

// Save the given config to NVS. Returns true on success.
bool net_config_save(const net_config_t *cfg);

// Get the live network state for the web UI status panel.
// All output buffers should be at least 16 chars; hostname_out >= 24.
// Returns true if interface is up and has an IP.
bool net_get_status(char *ip_out, char *mask_out, char *gw_out,
                    char *dns_out, char *hostname_out,
                    bool *is_static_out, bool *fallback_active_out,
                    bool *link_up_out);

#ifdef __cplusplus
}
#endif