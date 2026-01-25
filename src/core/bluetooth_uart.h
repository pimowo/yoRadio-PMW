#pragma once
#include <stdint.h>
// ACK timeout for play/pause (milliseconds)
// Slightly increased to give phones a bit more time to reply
#define BT_ACK_TIMEOUT_MS 7000
// Heartbeat timeout: consider BT disconnected if no messages for this period (ms)
// Increased to 120s to avoid spurious disconnects on flaky links
#define BT_HEARTBEAT_TIMEOUT_MS 120000

// runtime-configurable timeouts (defaults defined by macros above)
extern uint32_t bt_ack_timeout_ms;
extern uint32_t bt_heartbeat_timeout_ms;

void bluetooth_handle_line(const char *line);
