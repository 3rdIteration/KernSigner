#include "smartcard_transport.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "smartcard_ccid.h"
#include "smartcard_pn5180.h"

#ifndef CONFIG_KSIG_SEC1210_ENABLED
#define CONFIG_KSIG_SEC1210_ENABLED 0
#endif

#if CONFIG_KSIG_SEC1210_ENABLED
#include "smartcard_sec1210.h"
#endif

static const char *TAG = "KSIG_CARD_TX";

/* Priority order for auto-detection:
 * 1. NFC PN5180 (if enabled)
 * 2. UART SEC1210 (if enabled)
 * 3. USB CCID (always available as fallback) */

#if CONFIG_KSIG_PN5180_ENABLED
static smartcard_transport_t s_selected = SMARTCARD_TRANSPORT_NFC_PN5180;
static smartcard_transport_t s_active = SMARTCARD_TRANSPORT_NFC_PN5180;
#elif CONFIG_KSIG_SEC1210_ENABLED
static smartcard_transport_t s_selected = SMARTCARD_TRANSPORT_UART_SEC1210;
static smartcard_transport_t s_active = SMARTCARD_TRANSPORT_UART_SEC1210;
#else
static smartcard_transport_t s_selected = SMARTCARD_TRANSPORT_AUTO;
static smartcard_transport_t s_active = SMARTCARD_TRANSPORT_USB_CCID;
#endif
static bool s_active_valid;

#define SMARTCARD_TRANSPORT_DEFAULT_TIMEOUT_MS 30000U
#define SMARTCARD_TRANSPORT_USB_FALLBACK_TIMEOUT_MS 3000U

static uint32_t default_timeout(uint32_t timeout_ms) {
  return timeout_ms ? timeout_ms : SMARTCARD_TRANSPORT_DEFAULT_TIMEOUT_MS;
}

static uint32_t usb_fallback_probe_timeout(uint32_t timeout_ms) {
  uint32_t timeout = default_timeout(timeout_ms);
  return timeout > SMARTCARD_TRANSPORT_USB_FALLBACK_TIMEOUT_MS
             ? SMARTCARD_TRANSPORT_USB_FALLBACK_TIMEOUT_MS
             : timeout;
}

static bool pn5180_hw_present(void) {
  smartcard_pn5180_report_t report;
  smartcard_pn5180_snapshot(&report);
  return report.initialized && report.pn5180_present;
}

static esp_err_t probe_pn5180_primary(uint32_t timeout_ms,
                                      bool *hw_present) {
  esp_err_t err = smartcard_pn5180_probe(timeout_ms);
  bool present = pn5180_hw_present();
  if (hw_present)
    *hw_present = present;

  if (err == ESP_OK || present) {
    s_active = SMARTCARD_TRANSPORT_NFC_PN5180;
    s_active_valid = true;
  } else {
    s_active_valid = false;
  }

  return err;
}

static esp_err_t probe_usb_fallback(uint32_t timeout_ms) {
  esp_err_t err = smartcard_ccid_probe(usb_fallback_probe_timeout(timeout_ms));
  if (err == ESP_OK) {
    s_active = SMARTCARD_TRANSPORT_USB_CCID;
    s_active_valid = true;
  }
  return err;
}

#if CONFIG_KSIG_SEC1210_ENABLED
static bool sec1210_hw_initialized(void) {
  smartcard_sec1210_report_t report;
  smartcard_sec1210_snapshot(&report);
  return report.initialized;
}

static esp_err_t probe_sec1210_primary(uint32_t timeout_ms, bool *ready) {
  esp_err_t err = smartcard_sec1210_probe(timeout_ms);
  bool initialized = sec1210_hw_initialized();
  if (ready)
    *ready = (err == ESP_OK || initialized);

  if (err == ESP_OK || initialized) {
    s_active = SMARTCARD_TRANSPORT_UART_SEC1210;
    s_active_valid = true;
  } else {
    s_active_valid = false;
  }
  return err;
}

static esp_err_t probe_sec1210_fallback(uint32_t timeout_ms) {
  esp_err_t err = smartcard_sec1210_probe(timeout_ms);
  if (err == ESP_OK) {
    s_active = SMARTCARD_TRANSPORT_UART_SEC1210;
    s_active_valid = true;
  }
  return err;
}
#endif /* CONFIG_KSIG_SEC1210_ENABLED */

const char *smartcard_transport_name(smartcard_transport_t transport) {
  switch (transport) {
  case SMARTCARD_TRANSPORT_AUTO:
    return "Auto";
  case SMARTCARD_TRANSPORT_USB_CCID:
    return "USB CCID";
  case SMARTCARD_TRANSPORT_NFC_PN5180:
    return "NFC PN5180";
  case SMARTCARD_TRANSPORT_UART_SEC1210:
    return "UART SEC1210";
  default:
    return "Unknown";
  }
}

void smartcard_transport_set(smartcard_transport_t transport) {
  if (transport > SMARTCARD_TRANSPORT_UART_SEC1210)
    transport = SMARTCARD_TRANSPORT_AUTO;
  s_selected = transport;
  s_active_valid = false;
}

smartcard_transport_t smartcard_transport_get(void) { return s_selected; }

smartcard_transport_t smartcard_transport_active(void) {
  return s_active_valid ? s_active : s_selected;
}

esp_err_t smartcard_transport_probe(uint32_t timeout_ms) {
  const uint32_t timeout = default_timeout(timeout_ms);

#if CONFIG_KSIG_SEC1210_ENABLED
  /* SEC1210 explicit mode */
  if (s_selected == SMARTCARD_TRANSPORT_UART_SEC1210) {
    bool ready = false;
    esp_err_t err = probe_sec1210_primary(timeout, &ready);
    if (err == ESP_OK || ready) {
      ESP_LOGI(TAG, "Auto-selected UART SEC1210 smartcard transport");
      return ESP_OK;
    }
    /* SEC1210 failed: fall back to USB CCID */
    ESP_LOGW(TAG, "SEC1210 unavailable; trying USB CCID fallback");
    esp_err_t usb_err = probe_usb_fallback(timeout);
    if (usb_err == ESP_OK) {
      ESP_LOGW(TAG, "Auto-selected USB CCID fallback because SEC1210 is unavailable");
      return ESP_OK;
    }
    return usb_err;
  }
#endif

  if (s_selected == SMARTCARD_TRANSPORT_USB_CCID) {
    esp_err_t err = smartcard_ccid_probe(timeout);
    if (err == ESP_OK) {
      s_active = SMARTCARD_TRANSPORT_USB_CCID;
      s_active_valid = true;
    } else {
      s_active_valid = false;
    }
    return err;
  }

#if CONFIG_KSIG_PN5180_ENABLED
  /* PN5180 explicit mode */
  if (s_selected == SMARTCARD_TRANSPORT_NFC_PN5180) {
    bool pn5180_present = false;
    esp_err_t pn5180_err = probe_pn5180_primary(timeout, &pn5180_present);
    if (pn5180_err == ESP_OK)
      return ESP_OK;

    if (pn5180_present) {
      ESP_LOGW(TAG, "PN5180 present but no NFC smartcard ready; trying fallbacks");
#if CONFIG_KSIG_SEC1210_ENABLED
      esp_err_t sec_err = probe_sec1210_fallback(timeout);
      if (sec_err == ESP_OK) return ESP_OK;
#endif
      esp_err_t usb_err = probe_usb_fallback(timeout);
      if (usb_err == ESP_OK)
        return ESP_OK;
      return pn5180_err;
    }

    ESP_LOGW(TAG, "PN5180 unavailable; trying fallbacks");
#if CONFIG_KSIG_SEC1210_ENABLED
    esp_err_t sec_err = probe_sec1210_fallback(timeout);
    if (sec_err == ESP_OK) return ESP_OK;
#endif
    esp_err_t usb_err = probe_usb_fallback(timeout);
    if (usb_err == ESP_OK)
      return ESP_OK;
    return pn5180_err != ESP_ERR_NOT_FOUND ? pn5180_err : usb_err;
  }
#endif /* CONFIG_KSIG_PN5180_ENABLED */

  /* Auto-detection: try PN5180 → SEC1210 → USB CCID */
#if CONFIG_KSIG_PN5180_ENABLED
  bool pn5180_present = false;
  esp_err_t pn5180_err = probe_pn5180_primary(timeout, &pn5180_present);
  if (pn5180_err == ESP_OK) {
    ESP_LOGI(TAG, "Auto-selected NFC PN5180 smartcard transport");
    return ESP_OK;
  }
  if (pn5180_present) {
    ESP_LOGW(TAG, "PN5180 present but no NFC smartcard ready; trying fallbacks");
#if CONFIG_KSIG_SEC1210_ENABLED
    esp_err_t sec_err = probe_sec1210_fallback(timeout);
    if (sec_err == ESP_OK) {
      ESP_LOGW(TAG, "Auto-selected UART SEC1210 because PN5180 has no card ready");
      return ESP_OK;
    }
#endif
    esp_err_t usb_err = probe_usb_fallback(timeout);
    if (usb_err == ESP_OK) {
      ESP_LOGW(TAG, "Auto-selected USB CCID fallback because PN5180 has no NFC card ready");
      return ESP_OK;
    }
    return pn5180_err;
  }
#endif

#if CONFIG_KSIG_SEC1210_ENABLED
  bool sec1210_ready = false;
  esp_err_t sec_err = probe_sec1210_primary(timeout, &sec1210_ready);
  if (sec_err == ESP_OK || sec1210_ready) {
    ESP_LOGI(TAG, "Auto-selected UART SEC1210 smartcard transport");
    return ESP_OK;
  }
#endif

  esp_err_t usb_err = probe_usb_fallback(timeout);
  if (usb_err == ESP_OK) {
#if CONFIG_KSIG_PN5180_ENABLED
    ESP_LOGW(TAG, "Auto-selected USB CCID fallback because PN5180 is unavailable");
#elif CONFIG_KSIG_SEC1210_ENABLED
    ESP_LOGW(TAG, "Auto-selected USB CCID fallback because SEC1210 is unavailable");
#else
    ESP_LOGW(TAG, "Using USB CCID as default transport");
#endif
    return ESP_OK;
  }

#if CONFIG_KSIG_PN5180_ENABLED && CONFIG_KSIG_SEC1210_ENABLED
  ESP_LOGW(TAG, "No smartcard transport ready: PN5180=%s SEC1210=%s USB=%s",
           esp_err_to_name(pn5180_err), esp_err_to_name(sec_err), esp_err_to_name(usb_err));
#elif CONFIG_KSIG_SEC1210_ENABLED
  ESP_LOGW(TAG, "No smartcard transport ready: SEC1210=%s USB=%s",
           esp_err_to_name(sec_err), esp_err_to_name(usb_err));
#else
  ESP_LOGW(TAG, "No smartcard transport ready: USB=%s", esp_err_to_name(usb_err));
#endif
#if CONFIG_KSIG_PN5180_ENABLED
  return pn5180_err != ESP_ERR_NOT_FOUND ? pn5180_err : usb_err;
#elif CONFIG_KSIG_SEC1210_ENABLED
  return sec_err != ESP_ERR_NOT_FOUND ? sec_err : usb_err;
#else
  return usb_err;
#endif
}

esp_err_t smartcard_transport_transmit_apdu(const uint8_t *apdu,
                                            size_t apdu_len,
                                            uint8_t *response,
                                            size_t response_cap,
                                            size_t *response_len, uint16_t *sw,
                                            uint32_t timeout_ms) {
  if (!s_active_valid) {
    esp_err_t probe_err = smartcard_transport_probe(timeout_ms);
    if (probe_err != ESP_OK)
      return probe_err;
  }

  esp_err_t err = ESP_ERR_INVALID_STATE;

#if CONFIG_KSIG_SEC1210_ENABLED
  if (s_active == SMARTCARD_TRANSPORT_UART_SEC1210) {
    err = smartcard_sec1210_transmit_apdu(apdu, apdu_len, response,
                                           response_cap, response_len, sw,
                                           timeout_ms);
  } else
#endif

#if CONFIG_KSIG_PN5180_ENABLED
  if (s_active == SMARTCARD_TRANSPORT_NFC_PN5180) {
    err = smartcard_pn5180_transmit_apdu(apdu, apdu_len, response,
                                         response_cap, response_len, sw,
                                         timeout_ms);
  } else
#endif
  {
    err = smartcard_ccid_transmit_apdu(apdu, apdu_len, response, response_cap,
                                       response_len, sw, timeout_ms);
  }

  if (err != ESP_OK)
    s_active_valid = false;
  return err;
}

void smartcard_transport_format_report(char *out, size_t out_len) {
  if (!out || out_len == 0)
    return;

  char usb[512] = {0};
  char pn5180[512] = {0};
  char sec1210[512] = {0};
  smartcard_ccid_format_report(usb, sizeof(usb));
  smartcard_pn5180_format_report(pn5180, sizeof(pn5180));
#if CONFIG_KSIG_SEC1210_ENABLED
  smartcard_sec1210_format_report(sec1210, sizeof(sec1210));
#endif

#if CONFIG_KSIG_SEC1210_ENABLED && CONFIG_KSIG_PN5180_ENABLED
  snprintf(out, out_len,
           "Selected: %s\nActive: %s%s\n\nUART SEC1210\n%s\n\nNFC PN5180\n%s\n\nUSB CCID fallback\n%s",
           smartcard_transport_name(s_selected),
           s_active_valid ? smartcard_transport_name(s_active) : "None",
           s_active_valid ? "" : " (not probed)", sec1210, pn5180, usb);
#elif CONFIG_KSIG_SEC1210_ENABLED
  snprintf(out, out_len,
           "Selected: %s\nActive: %s%s\n\nUART SEC1210\n%s\n\nUSB CCID fallback\n%s",
           smartcard_transport_name(s_selected),
           s_active_valid ? smartcard_transport_name(s_active) : "None",
           s_active_valid ? "" : " (not probed)", sec1210, usb);
#elif CONFIG_KSIG_PN5180_ENABLED
  snprintf(out, out_len,
           "Selected: %s\nActive: %s%s\n\nNFC PN5180\n%s\n\nUSB CCID fallback\n%s",
           smartcard_transport_name(s_selected),
           s_active_valid ? smartcard_transport_name(s_active) : "None",
           s_active_valid ? "" : " (not probed)", pn5180, usb);
#else
  snprintf(out, out_len,
           "Selected: %s\nActive: %s%s\n\nUSB CCID\n%s",
           smartcard_transport_name(s_selected),
           s_active_valid ? smartcard_transport_name(s_active) : "None",
           s_active_valid ? "" : " (not probed)", usb);
#endif
}
