#ifndef SMARTCARD_SEC1210_H
#define SMARTCARD_SEC1210_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/* SEC1210 UART CCID constants */
#define SMARTCARD_SEC1210_SYNC        0x03U
#define SMARTCARD_SEC1210_CTRL_ACK    0x06U
#define SMARTCARD_SEC1210_CTRL_NAK    0x15U
#define SMARTCARD_SEC1210_CARD_ABSENT 0x02U
#define SMARTCARD_SEC1210_CARD_PRESENT 0x03U

#define SMARTCARD_SEC1210_ATR_MAX_LEN 64U
#define SMARTCARD_SEC1210_RESPONSE_MAX_LEN 256U
#define SMARTCARD_SEC1210_APDU_MAX_LEN 260U

/* CCID message minimum length (same as USB CCID) */
#define SMARTCARD_SEC1210_CCID_MSG_MIN_LEN 10U

typedef enum {
  SMARTCARD_SEC1210_STATE_IDLE,
  SMARTCARD_SEC1210_STATE_STARTING,
  SMARTCARD_SEC1210_STATE_WAITING,
  SMARTCARD_SEC1210_STATE_READY,
  SMARTCARD_SEC1210_STATE_ATR_OK,
  SMARTCARD_SEC1210_STATE_APPLET_OK,
  SMARTCARD_SEC1210_STATE_PASS,
  SMARTCARD_SEC1210_STATE_TIMEOUT,
  SMARTCARD_SEC1210_STATE_FAIL,
} smartcard_sec1210_state_t;

typedef struct {
  smartcard_sec1210_state_t state;
  bool initialized;
  bool card_present;
  uint32_t ccid_protocols;
  uint32_t ccid_features;
  uint32_t ccid_max_msg_len;
  uint32_t ccid_max_ifsd;
  bool t1_present;
  char detail[160];
  uint8_t atr[SMARTCARD_SEC1210_ATR_MAX_LEN];
  size_t atr_len;
  uint8_t response[SMARTCARD_SEC1210_RESPONSE_MAX_LEN];
  size_t response_len;
  uint16_t sw;
} smartcard_sec1210_report_t;

/**
 * Initialize the SEC1210 UART driver.
 *
 * Configures the ESP-IDF UART peripheral with pins and baud rate from
 * Kconfig. Must be called before any other sec1210 functions.
 *
 * @return ESP_OK on success, ESP_ERR_NO_MEM if UART resources unavailable
 */
esp_err_t smartcard_sec1210_start(void);

/**
 * Probe the SEC1210 reader: power on card, read ATR, identify applet.
 *
 * This function performs the full discovery sequence:
 * 1. Send RDR_IccPowerOn to get ATR
 * 2. Parse ATR for T=1 parameters
 * 3. Set T=1 parameters if needed
 * 4. SELECT Satochip / SeedKeeper / MF applets
 * 5. GET STATUS to confirm card readiness
 *
 * @param timeout_ms Maximum time to wait for card insertion/response
 * @return ESP_OK if card and applet ready, error code otherwise
 */
esp_err_t smartcard_sec1210_probe(uint32_t timeout_ms);

/**
 * Transmit one APDU through the SEC1210 smart card.
 *
 * Uses the CCID protocol over UART to send an APDU and receive the response.
 * Supports both T=0 and T=1 protocols based on ATR negotiation.
 *
 * @param apdu APDU bytes to send
 * @param apdu_len Length of APDU in bytes
 * @param response Buffer for response data
 * @param response_cap Capacity of response buffer
 * @param response_len Output: actual response length
 * @param sw Output: SW1/SW2 status word
 * @param timeout_ms Command timeout in milliseconds
 * @return ESP_OK on success, error code on failure
 */
esp_err_t smartcard_sec1210_transmit_apdu(const uint8_t *apdu, size_t apdu_len,
                                          uint8_t *response,
                                          size_t response_cap,
                                          size_t *response_len, uint16_t *sw,
                                          uint32_t timeout_ms);

/**
 * Get current driver state snapshot.
 * @param out Pointer to report structure to fill
 */
void smartcard_sec1210_snapshot(smartcard_sec1210_report_t *out);

/**
 * Format a human-readable status report.
 * @param out Output buffer
 * @param out_len Output buffer length
 */
void smartcard_sec1210_format_report(char *out, size_t out_len);

const char *smartcard_sec1210_state_name(smartcard_sec1210_state_t state);

#endif /* SMARTCARD_SEC1210_H */