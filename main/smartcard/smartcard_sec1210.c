#include "smartcard_sec1210.h"

#include "i18n/i18n.h"

#ifdef SIMULATOR

#include <stdio.h>
#include <string.h>

static smartcard_sec1210_report_t s_report = {
    .state = SMARTCARD_SEC1210_STATE_FAIL,
    .detail = "Simulator does not access UART; use hardware to probe SEC1210.",
};

esp_err_t smartcard_sec1210_start(void) { return ESP_ERR_NOT_SUPPORTED; }

esp_err_t smartcard_sec1210_probe(uint32_t timeout_ms) {
    (void)timeout_ms;
    s_report.state = SMARTCARD_SEC1210_STATE_FAIL;
    snprintf(s_report.detail, sizeof(s_report.detail), "%s",
             "Simulator does not access UART; SEC1210 is hardware-only.");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t smartcard_sec1210_transmit_apdu(const uint8_t *apdu, size_t apdu_len,
                                          uint8_t *response,
                                          size_t response_cap,
                                          size_t *response_len, uint16_t *sw,
                                          uint32_t timeout_ms) {
    (void)apdu;
    (void)apdu_len;
    (void)response;
    (void)response_cap;
    (void)response_len;
    (void)sw;
    (void)timeout_ms;
    return ESP_ERR_NOT_SUPPORTED;
}

void smartcard_sec1210_snapshot(smartcard_sec1210_report_t *out) {
    if (out)
        *out = s_report;
}

const char *smartcard_sec1210_state_name(smartcard_sec1210_state_t state) {
    switch (state) {
    case SMARTCARD_SEC1210_STATE_IDLE: return "Idle";
    case SMARTCARD_SEC1210_STATE_STARTING: return "Starting";
    case SMARTCARD_SEC1210_STATE_WAITING: return "Waiting";
    case SMARTCARD_SEC1210_STATE_READY: return "Ready";
    case SMARTCARD_SEC1210_STATE_ATR_OK: return "ATR OK";
    case SMARTCARD_SEC1210_STATE_APPLET_OK: return "Applet OK";
    case SMARTCARD_SEC1210_STATE_PASS: return "Pass";
    case SMARTCARD_SEC1210_STATE_TIMEOUT: return "Timeout";
    case SMARTCARD_SEC1210_STATE_FAIL: return "Fail";
    default: return "Unknown";
    }
}

void smartcard_sec1210_format_report(char *out, size_t out_len) {
    if (!out || out_len == 0)
        return;
    snprintf(out, out_len, "SEC1210 UART: simulator not supported");
}

#else /* !SIMULATOR */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "smartcard_ccid.h"

#define TAG "KSIG_SEC1210"

/* CCID protocol constants (same as USB CCID) */
#define SMARTCARD_SEC1210_CCID_PROTOCOL_T0 0x00000001U
#define SMARTCARD_SEC1210_CCID_PROTOCOL_T1 0x00000002U
#define SMARTCARD_SEC1210_CCID_FEATURE_TPDU_EXCHANGE 0x00010000U
#define SMARTCARD_SEC1210_CCID_FEATURE_SHORT_APDU_EXCHANGE 0x00020000U

#define SMARTCARD_SEC1210_CARD_SW_OK 0x9000U

/* CCID command codes */
#define SMARTCARD_SEC1210_RDR_TO_PC_DATA_BLOCK 0x80
#define SMARTCARD_SEC1210_RDR_TO_PC_NOTIFY_SLOT_CHANGE 0x50
#define SMARTCARD_SEC1210_PC_TO_RDR_ICCPOWERON 0x62
#define SMARTCARD_SEC1210_PC_TO_RDR_SETPARAMETERS 0x61
#define SMARTCARD_SEC1210_PC_TO_RDF_XFRBLOCK 0x6F
#define SMARTCARD_SEC1210_PC_TO_RDR_ESCAPE 0x6B

/* CCID response header, 10 bytes (CCID/src/commands.h: CCID_HEADER_SIZE):
 *   bMessageType[0] dwLength[1..4] bSlot[5] bSeq[6]
 *   bStatus[7] bError[8] bChainParameter[9] abData[10..] */
#define SMARTCARD_SEC1210_CCID_HEADER_SIZE 10U
#define SMARTCARD_SEC1210_CCID_OFF_STATUS 7U
#define SMARTCARD_SEC1210_CCID_OFF_ERROR 8U

/* T=1 constants */
#define SMARTCARD_SEC1210_T1_MAX_INF_LEN 254U
#define SMARTCARD_SEC1210_ATR_TA1_DEFAULT 0x11U
#define SMARTCARD_SEC1210_ATR_TC1_DEFAULT 0x00U
#define SMARTCARD_SEC1210_ATR_T1_WAITING_DEFAULT 0x4DU
#define SMARTCARD_SEC1210_ATR_T1_TCCKS_DEFAULT 0x10U
#define SMARTCARD_SEC1210_ATR_T1_IFSC_DEFAULT 0x20U

/* APDU sequences for applet identification */
static const uint8_t k_select_satochip_apdu[] = {
    0x00, 0xA4, 0x04, 0x00, 0x08, 0x53, 0x61, 0x74,
    0x6f, 0x43, 0x68, 0x69, 0x70};
static const uint8_t k_select_seedkeeper_apdu[] = {
    0x00, 0xA4, 0x04, 0x00, 0x0A, 0x53, 0x65, 0x65, 0x64,
    0x4b, 0x65, 0x65, 0x70, 0x65, 0x72};
static const uint8_t k_select_mf_apdu[] = {0x00, 0xA4, 0x00, 0x00, 0x00};
static const uint8_t k_get_status_apdu[] = {0xB0, 0x3C, 0x00, 0x00, 0x00};

/* LRC (XOR checksum) for UART framing */
static uint8_t sec1210_lrc(const uint8_t *data, size_t len) {
    uint8_t lrc = 0;
    for (size_t i = 0; i < len; i++)
        lrc ^= data[i];
    return lrc;
}

/* Little-endian helpers */
static uint16_t read_le16(const uint8_t *buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static uint32_t read_le32(const uint8_t *buf) {
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

static void write_le32(uint8_t *buf, uint32_t value) {
    buf[0] = value & 0xFF;
    buf[1] = (value >> 8) & 0xFF;
    buf[2] = (value >> 16) & 0xFF;
    buf[3] = (value >> 24) & 0xFF;
}

/* T=1 block building */
static uint8_t ccid_t1_lrc(const uint8_t *data, size_t len) {
    uint8_t lrc = 0;
    for (size_t i = 0; i < len; i++)
        lrc ^= data[i];
    return lrc;
}

static size_t ccid_build_t1_block(uint8_t *out, uint8_t pcb,
                                  const uint8_t *inf, size_t inf_len) {
    if (!out || inf_len > SMARTCARD_SEC1210_T1_MAX_INF_LEN)
        return 0;
    out[0] = 0x00; /* NAD */
    out[1] = pcb;
    out[2] = (uint8_t)inf_len;
    if (inf_len > 0 && inf)
        memcpy(&out[3], inf, inf_len);
    out[3 + inf_len] = ccid_t1_lrc(out, 3 + inf_len);
    return 4 + inf_len;
}

static size_t ccid_build_t1_r_block(uint8_t *out, uint8_t nr) {
    if (!out)
        return 0;
    uint8_t pcb = 0x80 | ((nr & 0x01U) << 4);
    return ccid_build_t1_block(out, pcb, NULL, 0);
}

static bool t1_is_i_block(uint8_t pcb) { return (pcb & 0x80U) == 0x00U; }
static bool t1_is_r_block(uint8_t pcb) { return (pcb & 0xC0U) == 0x80U; }
static uint8_t t1_i_ns(uint8_t pcb) { return (pcb >> 6) & 0x01U; }

/* T=1 state for this context */
typedef struct {
    uint8_t host_ns;
    uint8_t expected_card_ns;
    bool command_accepted;
    uint8_t exchange_steps;
    uint8_t last_i_block[SMARTCARD_SEC1210_T1_MAX_INF_LEN + 4];
    size_t last_i_block_len;
    uint8_t response[SMARTCARD_SEC1210_RESPONSE_MAX_LEN];
    size_t response_len;
} t1_state_t;

/* SEC1210 context */
typedef struct {
    bool initialized;
    smartcard_sec1210_report_t report;
    uint8_t seq;
    bool card_powered;
    bool atr_parsed;
    bool apdu_active;
    bool apdu_completed;
    esp_err_t apdu_result;
    uint8_t *apdu_resp;
    size_t apdu_resp_cap;
    size_t apdu_resp_len;
    uint16_t apdu_sw;
    uint8_t custom_apdu_buf[SMARTCARD_SEC1210_APDU_MAX_LEN];
    size_t custom_apdu_len;
    bool want_applet_identify;
    size_t apdu_step;

    /* T=1 state */
    t1_state_t t1;
    uint8_t atr_ta1;
    uint8_t atr_tc1;
    uint8_t atr_t1_waiting;
    uint8_t atr_t1_ifsc;
    uint8_t atr_t1_tccks;
    bool atr_t1_present;
    bool tpdu_exchange;

    /* CCID descriptor info */
    uint32_t dw_protocols;
    uint32_t dw_features;
    uint32_t dw_max_ifsd;
    uint32_t dw_default_clock;
    uint32_t dw_max_data_rate;

    /* RX diagnostics, shown on screen to distinguish "reader silent" from
     * "reader replying but misparsed". */
    uint32_t rx_bytes_total;
    uint8_t rx_first[16];
    uint8_t rx_first_len;
} sec1210_ctx_t;

static sec1210_ctx_t s_ctx = {
    .seq = 1,
};

/* UART write: prepend SYNC+CTRL, append LRC */
static esp_err_t sec1210_uart_write(const uint8_t *ccid_cmd, size_t len) {
    uint8_t frame[len + 3];
    frame[0] = SMARTCARD_SEC1210_SYNC; /* SYNC */
    frame[1] = SMARTCARD_SEC1210_CTRL_ACK; /* ACK */
    memcpy(frame + 2, ccid_cmd, len);
    frame[len + 2] = sec1210_lrc(frame, len + 2);

    uart_port_t uart_num = CONFIG_KSIG_SEC1210_UART_NUM;
    esp_err_t err = uart_write_bytes(uart_num, (const char *)frame, len + 3);
    return (err >= 0) ? ESP_OK : err;
}

/* Record received bytes for the on-screen diagnostics report. */
static void sec1210_note_rx(const uint8_t *data, size_t len) {
    s_ctx.rx_bytes_total += (uint32_t)len;
    for (size_t i = 0; i < len && s_ctx.rx_first_len < sizeof(s_ctx.rx_first); i++)
        s_ctx.rx_first[s_ctx.rx_first_len++] = data[i];
}

/* Read a single byte with timeout, returns ESP_OK or ESP_ERR_TIMEOUT */
static esp_err_t sec1210_uart_read_byte(uint8_t *out, uint32_t timeout_ms) {
    uart_port_t uart_num = CONFIG_KSIG_SEC1210_UART_NUM;
    int n = uart_read_bytes(uart_num, out, 1, pdMS_TO_TICKS(timeout_ms));
    if (n != 1)
        return ESP_ERR_TIMEOUT;
    sec1210_note_rx(out, 1);
    return ESP_OK;
}

/* Read CCID response from SEC1210 over UART.
 *
 * Protocol (from Linux ccid_serial.c):
 * 1. Skip NotifySlotChange bytes (0x50 + status)
 * 2. Wait for SYNC (0x03)
 * 3. Expect CTRL (ACK=0x06 or NAK=0x15)
 * 4. If NAK: read LRC, return ESP_ERR_NOT_FOUND
 * 5. Read CCID response header (5 bytes) to get message length
 * 6. Read remaining data
 * 7. Verify LRC
 * 8. Handle echo: if this echoes our last command, discard and retry
 */
static esp_err_t sec1210_uart_read_response(sec1210_ctx_t *ctx,
                                             uint8_t *buf, size_t cap,
                                             size_t *out_len) {
    uint8_t b;
    uart_port_t uart_num = CONFIG_KSIG_SEC1210_UART_NUM;

start:
    /* Step 1: Look for SYNC byte or slot change notification */
    while (ESP_OK == sec1210_uart_read_byte(&b, ctx->tpdu_exchange ? 5000 : 2000)) {
        if (b == SMARTCARD_SEC1210_RDR_TO_PC_NOTIFY_SLOT_CHANGE) {
            /* Slot change notification: read status byte. Only accept a valid
             * bmSlotIccState - 0x50 is also ASCII 'P', so stray text on the
             * line (e.g. console output looping back on a shared pad) would
             * otherwise be misreported as a genuine card-absent notification. */
            if (ESP_OK != sec1210_uart_read_byte(&b, 1000))
                return ESP_ERR_TIMEOUT;
            if (b != SMARTCARD_SEC1210_CARD_ABSENT &&
                b != SMARTCARD_SEC1210_CARD_PRESENT) {
                ESP_LOGD(TAG, "Ignoring bogus slot state: 0x%02X", b);
                goto start;
            }
            ctx->report.card_present = (b == SMARTCARD_SEC1210_CARD_PRESENT);
            ESP_LOGI(TAG, "Slot change: card %s",
                     ctx->report.card_present ? "present" : "absent");
            /* Continue looking for response */
            goto start;
        }

        if (b == SMARTCARD_SEC1210_SYNC) {
            /* Step 2-3: Read CTRL byte */
            if (ESP_OK != sec1210_uart_read_byte(&b, 1000))
                return ESP_ERR_TIMEOUT;

            if (b == SMARTCARD_SEC1210_CTRL_NAK) {
                /* NAK: read LRC and return */
                uint8_t lrc_check;
                if (ESP_OK != sec1210_uart_read_byte(&lrc_check, 1000))
                    return ESP_ERR_TIMEOUT;
                if (lrc_check == (SMARTCARD_SEC1210_SYNC ^ SMARTCARD_SEC1210_CTRL_NAK)) {
                    ESP_LOGD(TAG, "NACK received from reader");
                    return ESP_ERR_NOT_FOUND;
                }
                return ESP_ERR_INVALID_CRC;
            }

            if (b != SMARTCARD_SEC1210_CTRL_ACK) {
                ESP_LOGE(TAG, "Unexpected CTRL byte: 0x%02X", b);
                return ESP_ERR_INVALID_RESPONSE;
            }

            /* Step 5: Read the first 5 bytes (bMessageType + dwLength LE32) */
            uint8_t header[5];
            for (int i = 0; i < 5; i++) {
                if (ESP_OK != sec1210_uart_read_byte(&header[i], 2000))
                    return ESP_ERR_TIMEOUT;
            }

            /* Total frame is the whole CCID header plus dwLength, not 5 +
             * dwLength (ccid_serial.c: to_read = CCID_RESPONSE_HEADER_SIZE +
             * dw2i(buffer, 1)). Reading 5 + dwLength leaves 5 bytes of every
             * frame unread and desynchronises the stream. */
            uint32_t msg_len = read_le32(header + 1);
            uint32_t total_len = SMARTCARD_SEC1210_CCID_HEADER_SIZE + msg_len;

            if (total_len > cap) {
                ESP_LOGE(TAG, "Response too large: %lu > %zu",
                         (unsigned long)total_len, cap);
                return ESP_ERR_NO_MEM;
            }

            /* Copy header to output */
            memcpy(buf, header, 5);

            /* Step 6: Read the rest of the frame (5 bytes already consumed) */
            size_t remaining = total_len - 5;
            if (remaining > 0) {
                int read_count = uart_read_bytes(uart_num, buf + 5, remaining,
                                                 pdMS_TO_TICKS(2000));
                if (read_count < 0 || (size_t)read_count != remaining)
                    return ESP_ERR_TIMEOUT;
                sec1210_note_rx(buf + 5, remaining);
            }

            /* Step 7: Verify LRC. The reader picks it so that the running XOR
             * of the LRC byte and every frame byte equals SYNC ^ CTRL_ACK. */
            uint8_t lrc = 0;
            if (ESP_OK != sec1210_uart_read_byte(&lrc, 1000))
                return ESP_ERR_TIMEOUT;

            for (uint32_t i = 0; i < total_len; i++)
                lrc ^= buf[i];

            if (lrc != (SMARTCARD_SEC1210_SYNC ^ SMARTCARD_SEC1210_CTRL_ACK)) {
                ESP_LOGW(TAG, "LRC mismatch: got 0x%02X, expected 0x%02X", lrc,
                         (uint8_t)(SMARTCARD_SEC1210_SYNC ^ SMARTCARD_SEC1210_CTRL_ACK));
            }

            *out_len = total_len;

            /* Step 8: The SEC1210 does not echo commands back; ccid_serial.c
             * sets device.echo = false for both SEC1210 variants, so the first
             * complete frame is the response. */
            return ESP_OK;
        }

        /* Byte is 0x80-0xFF: T=0 time extension request, skip it */
        if (b >= 0x80) {
            ESP_LOGD(TAG, "Time extension request: 0x%02X", b);
            goto start;
        }

        /* Unknown byte, skip and continue looking */
        ESP_LOGD(TAG, "Skipping unknown byte: 0x%02X", b);
    }

    return ESP_ERR_TIMEOUT;
}

/* Build RDR_IccPowerOn CCID command */
static size_t build_power_on(uint8_t *buf, uint8_t slot, uint8_t seq,
                              uint8_t voltage) {
    memset(buf, 0, SMARTCARD_SEC1210_CCID_MSG_MIN_LEN);
    buf[0] = SMARTCARD_SEC1210_PC_TO_RDR_ICCPOWERON;
    write_le32(&buf[1], 0); /* dwLength = 0 (auto voltage) */
    buf[5] = slot; /* bSlot */
    buf[6] = seq; /* bSeq */
    buf[7] = voltage; /* bVoltage: 0x00=auto, 0x01=5V, 0x02=3V, 0x03=1.8V */
    return SMARTCARD_SEC1210_CCID_MSG_MIN_LEN;
}

/* Build RDR_SetParameters CCID command for T=1 */
static size_t build_set_parameters(sec1210_ctx_t *ctx, uint8_t *buf,
                                    uint8_t slot, uint8_t seq) {
    uint8_t ifsc = ctx->atr_t1_ifsc ? ctx->atr_t1_ifsc : SMARTCARD_SEC1210_ATR_T1_IFSC_DEFAULT;
    if (ctx->dw_max_ifsd > 0 && ifsc > (uint8_t)ctx->dw_max_ifsd)
        ifsc = (uint8_t)ctx->dw_max_ifsd;
    if (ifsc == 0)
        ifsc = SMARTCARD_SEC1210_ATR_T1_IFSC_DEFAULT;

    uint8_t params[7] = {0};
    params[0] = ctx->atr_ta1 ? ctx->atr_ta1 : SMARTCARD_SEC1210_ATR_TA1_DEFAULT; /* F/D */
    params[1] = ctx->atr_t1_tccks ? ctx->atr_t1_tccks : SMARTCARD_SEC1210_ATR_T1_TCCKS_DEFAULT; /* TCCKS */
    params[2] = ctx->atr_tc1; /* GT */
    params[3] = ctx->atr_t1_waiting ? ctx->atr_t1_waiting : SMARTCARD_SEC1210_ATR_T1_WAITING_DEFAULT; /* WI */
    params[4] = 0x00; /* bGuardT */
    params[5] = ifsc; /* bIFSC */
    params[6] = 0x00; /* NAD */

    memset(buf, 0, SMARTCARD_SEC1210_CCID_MSG_MIN_LEN + sizeof(params));
    buf[0] = SMARTCARD_SEC1210_PC_TO_RDR_SETPARAMETERS;
    write_le32(&buf[1], sizeof(params));
    buf[5] = slot;
    buf[6] = seq;
    buf[7] = 0x01; /* bNAD */
    memcpy(&buf[SMARTCARD_SEC1210_CCID_MSG_MIN_LEN], params, sizeof(params));

    ESP_LOGI(TAG, "SetParameters T=1: F/D=0x%02X TCCKS=0x%02X GT=%u WI=%u IFSC=%u",
             params[0], params[1], params[2], params[3], params[5]);
    return SMARTCARD_SEC1210_CCID_MSG_MIN_LEN + sizeof(params);
}

/* Build PC_to_RDR_Escape CCID command */
static size_t build_escape(uint8_t *buf, uint8_t slot, uint8_t seq,
                            const uint8_t *payload, size_t payload_len) {
    memset(buf, 0, SMARTCARD_SEC1210_CCID_MSG_MIN_LEN + payload_len);
    buf[0] = SMARTCARD_SEC1210_PC_TO_RDR_ESCAPE;
    write_le32(&buf[1], payload_len);
    buf[5] = slot;
    buf[6] = seq;
    buf[7] = 0x00;
    if (payload && payload_len > 0)
        memcpy(&buf[SMARTCARD_SEC1210_CCID_MSG_MIN_LEN], payload, payload_len);
    return SMARTCARD_SEC1210_CCID_MSG_MIN_LEN + payload_len;
}

/* Build RDR_XfrBlock CCID command */
static size_t build_xfr_block(uint8_t *buf, uint8_t slot, uint8_t seq,
                               const uint8_t *payload, size_t payload_len) {
    memset(buf, 0, SMARTCARD_SEC1210_CCID_MSG_MIN_LEN + payload_len);
    buf[0] = SMARTCARD_SEC1210_PC_TO_RDF_XFRBLOCK;
    write_le32(&buf[1], payload_len);
    buf[5] = slot;
    buf[6] = seq;
    buf[7] = 0x00;
    memcpy(&buf[SMARTCARD_SEC1210_CCID_MSG_MIN_LEN], payload, payload_len);
    return SMARTCARD_SEC1210_CCID_MSG_MIN_LEN + payload_len;
}

/* Parse T=1 parameters from ATR */
static void parse_atr_t_params(sec1210_ctx_t *ctx, const uint8_t *atr,
                                size_t atr_len) {
    ctx->atr_t1_present = false;
    ctx->atr_ta1 = SMARTCARD_SEC1210_ATR_TA1_DEFAULT;
    ctx->atr_tc1 = SMARTCARD_SEC1210_ATR_TC1_DEFAULT;
    ctx->atr_t1_waiting = SMARTCARD_SEC1210_ATR_T1_WAITING_DEFAULT;
    ctx->atr_t1_ifsc = SMARTCARD_SEC1210_ATR_T1_IFSC_DEFAULT;
    ctx->atr_t1_tccks = SMARTCARD_SEC1210_ATR_T1_TCCKS_DEFAULT;

    if (!atr || atr_len < 2)
        return;

    size_t offset = 2;
    uint8_t y = (atr[1] >> 4) & 0x0F;
    uint8_t protocol_for_group = 0;

    for (uint8_t group = 1; group < 8 && offset <= atr_len; group++) {
        bool have_ta = (y & 0x01U) != 0;
        bool have_tb = (y & 0x02U) != 0;
        bool have_tc = (y & 0x04U) != 0;
        uint8_t ta = 0;

        if (have_ta) {
            if (offset >= atr_len) return;
            ta = atr[offset++];
            if (group == 1) ctx->atr_ta1 = ta;
        }
        if (have_tb) {
            if (offset >= atr_len) return;
            offset++; /* tb value, not used for T=1 setup */
        }
        if (have_tc) {
            if (offset >= atr_len) return;
            uint8_t tc = atr[offset++];
            if (group == 1) ctx->atr_tc1 = tc;
        }

        if (protocol_for_group == 1 && group >= 2) {
            ctx->atr_t1_present = true;
            if (have_ta && ta != 0) ctx->atr_t1_ifsc = ta;
        }

        /* Look for TD byte after TA/TB/TC groups to find the next protocol */
        if (offset < atr_len) {
            uint8_t td = atr[offset];
            protocol_for_group = td & 0x0F;
            if (protocol_for_group == 1) ctx->atr_t1_present = true;
            y = (td >> 4) & 0x0F;
            offset++;
        } else {
            y = 0;
        }

        if (y == 0) break;
    }

    ESP_LOGI(TAG, "ATR parsed: T=1=%s, TA1=0x%02X, TC1=0x%02X, IFSC=%u",
             ctx->atr_t1_present ? "yes" : "no",
             ctx->atr_ta1, ctx->atr_tc1, ctx->atr_t1_ifsc);
}

/* Send CCID command and wait for response */
static esp_err_t sec1210_send_receive(sec1210_ctx_t *ctx,
                                       const uint8_t *cmd, size_t cmd_len,
                                       uint8_t *resp, size_t resp_cap,
                                       size_t *resp_len) {
    /* Write command with UART framing */
    esp_err_t err = sec1210_uart_write(cmd, cmd_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART write failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Read response */
    return sec1210_uart_read_response(ctx, resp, resp_cap, resp_len);
}

/* Parse CCID response status */
static bool parse_ccid_status(const uint8_t *resp, size_t len, uint8_t *status,
                               uint8_t *error) {
    if (len < SMARTCARD_SEC1210_CCID_HEADER_SIZE) return false;
    if (resp[0] != SMARTCARD_SEC1210_RDR_TO_PC_DATA_BLOCK &&
        resp[0] != SMARTCARD_SEC1210_RDR_TO_PC_NOTIFY_SLOT_CHANGE)
        return false;

    *status = resp[SMARTCARD_SEC1210_CCID_OFF_STATUS];
    if (error)
        *error = resp[SMARTCARD_SEC1210_CCID_OFF_ERROR];
    return true;
}

/* CCID bError names (CCID rev 1.1, section 6.2.6), for bring-up logs. */
static const char *ccid_error_name(uint8_t error) {
    switch (error) {
    case 0xFF: return "CMD_ABORTED";
    case 0xFE: return "ICC_MUTE";
    case 0xFD: return "XFR_PARITY_ERROR";
    case 0xFC: return "XFR_OVERRUN";
    case 0xFB: return "HW_ERROR";
    case 0xF8: return "BAD_ATR_TS";
    case 0xF7: return "BAD_ATR_TCK";
    case 0xF6: return "ICC_PROTOCOL_NOT_SUPPORTED";
    case 0xF5: return "ICC_CLASS_NOT_SUPPORTED";
    case 0xF4: return "PROCEDURE_BYTE_CONFLICT";
    case 0xF3: return "DEACTIVATED_PROTOCOL";
    case 0xF2: return "BUSY_WITH_AUTO_SEQUENCE";
    case 0xE0: return "CMD_SLOT_BUSY";
    case 0x00: return "CMD_NOT_SUPPORTED";
    default: return "unknown";
    }
}

/* bmICCStatus, bits 0-1 of bStatus */
static const char *ccid_icc_status_name(uint8_t status) {
    switch (status & 0x03U) {
    case 0x00: return "active";
    case 0x01: return "present, inactive";
    case 0x02: return "not present";
    default: return "RFU";
    }
}

/* Hex-dump a received CCID frame to the console. */
static void sec1210_log_frame(const char *what, const uint8_t *frame,
                              size_t len) {
    char hex[3 * 32 + 1];
    size_t pos = 0;
    for (size_t i = 0; i < len && i < 32; i++)
        pos += (size_t)snprintf(hex + pos, sizeof(hex) - pos, "%02X ", frame[i]);
    hex[pos] = '\0';
    ESP_LOGI(TAG, "%s (%u bytes): %s%s", what, (unsigned)len, hex,
             len > 32 ? "..." : "");
}

/* Extract ATR from RDR_IccPowerOn response */
static size_t extract_atr(const uint8_t *resp, size_t resp_len,
                           uint8_t *atr, size_t atr_cap) {
    if (resp_len < SMARTCARD_SEC1210_CCID_HEADER_SIZE + 4) return 0;

    uint32_t atr_len = read_le32(resp + 1);
    if (atr_len > atr_cap || atr_len > resp_len - SMARTCARD_SEC1210_CCID_HEADER_SIZE)
        atr_len = (uint32_t)(resp_len - SMARTCARD_SEC1210_CCID_HEADER_SIZE);

    memcpy(atr, resp + SMARTCARD_SEC1210_CCID_HEADER_SIZE, atr_len);
    return (size_t)atr_len;
}

/* Extract APDU response data and SW from CCID Data Block response */
static esp_err_t extract_apdu_response(const uint8_t *resp, size_t resp_len,
                                        uint8_t *out_buf, size_t out_cap,
                                        size_t *out_len, uint16_t *sw) {
    if (resp_len < SMARTCARD_SEC1210_CCID_HEADER_SIZE)
        return ESP_ERR_INVALID_RESPONSE;

    uint8_t status = resp[SMARTCARD_SEC1210_CCID_OFF_STATUS];
    if (status != 0x00) /* PROCESSED */
        return ESP_FAIL;

    uint32_t data_len = read_le32(resp + 1);
    size_t data_offset = SMARTCARD_SEC1210_CCID_HEADER_SIZE;

    /* bChainParameter is byte 9 of the CCID header */
    if (resp_len > 9 && (resp[9] & 0x80)) {
        /* More data chained - not expected in normal flow */
        ESP_LOGW(TAG, "Chained response detected");
    }

    /* The frame must carry the advertised payload, and an APDU response
     * always ends with SW1 SW2. */
    if (data_len < 2 || data_offset + data_len > resp_len)
        return ESP_ERR_INVALID_RESPONSE;

    if (data_len > out_cap)
        return ESP_ERR_NO_MEM;

    if (out_buf)
        memcpy(out_buf, resp + data_offset, data_len);
    if (out_len) *out_len = data_len;

    /* SW is last 2 bytes of data */
    const uint8_t *sw_ptr = resp + data_offset + data_len - 2;
    if (sw)
        *sw = ((uint16_t)sw_ptr[0] << 8) | sw_ptr[1];

    return ESP_OK;
}

/* Send one T=1 block in an XfrBlock and return the card's reply block. At
 * TPDU level the reader passes blocks through untouched. A WTX request
 * (S-block 0xC3) is answered here, so callers only see I- and R-blocks and
 * the S-blocks this does not handle. */
static esp_err_t t1_exchange_block(sec1210_ctx_t *ctx, const uint8_t *blk,
                                   size_t blk_len, uint8_t *rblk,
                                   size_t rblk_cap, size_t *rblk_len) {
    uint8_t cmd[SMARTCARD_SEC1210_CCID_MSG_MIN_LEN + SMARTCARD_SEC1210_T1_MAX_INF_LEN + 4];
    uint8_t resp[SMARTCARD_SEC1210_RESPONSE_MAX_LEN];
    uint8_t wtx[5]; /* NAD PCB LEN INF LRC */
    size_t resp_len = 0;

    for (int wtx_count = 0; wtx_count < 16; wtx_count++) {
        size_t cmd_len = build_xfr_block(cmd, 0, ctx->seq++, blk, blk_len);
        esp_err_t err = sec1210_send_receive(ctx, cmd, cmd_len, resp,
                                             sizeof(resp), &resp_len);
        if (err != ESP_OK)
            return err;

        uint8_t status = 0, error = 0;
        if (!parse_ccid_status(resp, resp_len, &status, &error))
            return ESP_ERR_INVALID_RESPONSE;
        if (status & 0x40U) {
            ESP_LOGW(TAG, "XfrBlock failed: bStatus=0x%02X bError=0x%02X (%s)",
                     status, error, ccid_error_name(error));
            return ESP_FAIL;
        }

        uint32_t data_len = read_le32(resp + 1);
        if (data_len < 4 || data_len > resp_len - SMARTCARD_SEC1210_CCID_HEADER_SIZE ||
            data_len > rblk_cap)
            return ESP_ERR_INVALID_RESPONSE;
        const uint8_t *r = resp + SMARTCARD_SEC1210_CCID_HEADER_SIZE;
        if ((size_t)r[2] + 4 != data_len || ccid_t1_lrc(r, data_len) != 0) {
            ESP_LOGW(TAG, "Bad T=1 block from card");
            sec1210_log_frame("  block", r, data_len);
            return ESP_ERR_INVALID_CRC;
        }

        if (r[1] != 0xC3) { /* not S(WTX request) */
            memcpy(rblk, r, data_len);
            *rblk_len = data_len;
            return ESP_OK;
        }

        /* S(WTX response) echoes the multiplier, then resend nothing: the
         * card's reply to the original block follows. */
        ESP_LOGD(TAG, "T=1 WTX x%u", r[3]);
        blk_len = ccid_build_t1_block(wtx, 0xE3, &r[3], 1);
        blk = wtx;
    }
    return ESP_ERR_TIMEOUT;
}

/* Transmit APDU using T=1 (ISO 7816-3 section 11) over a TPDU-level reader:
 * the host builds the blocks. N(S) for both sides persists across APDUs and
 * is reset at power-on. Handles send and receive chaining, IFS requests and
 * card R-blocks asking for retransmission. */
static esp_err_t transmit_apdu_t1(sec1210_ctx_t *ctx, const uint8_t *apdu,
                                   size_t apdu_len, uint8_t *response,
                                   size_t response_cap, size_t *response_len,
                                   uint16_t *sw) {
    uint8_t blk[SMARTCARD_SEC1210_T1_MAX_INF_LEN + 4];
    uint8_t rblk[SMARTCARD_SEC1210_T1_MAX_INF_LEN + 4];
    size_t blk_len, rblk_len = 0;
    size_t ifsc = ctx->atr_t1_ifsc ? ctx->atr_t1_ifsc : SMARTCARD_SEC1210_ATR_T1_IFSC_DEFAULT;
    size_t off = 0, out_len = 0;
    int retries = 0;
    esp_err_t err;

    if (!apdu || !response)
        return ESP_ERR_INVALID_ARG;
    if (ifsc > SMARTCARD_SEC1210_T1_MAX_INF_LEN)
        ifsc = SMARTCARD_SEC1210_T1_MAX_INF_LEN;

    /* Send phase: I-blocks, chained when the APDU exceeds IFSC. */
    for (;;) {
        size_t chunk = apdu_len - off;
        if (chunk > ifsc)
            chunk = ifsc;
        bool more = off + chunk < apdu_len;
        uint8_t pcb = (uint8_t)((ctx->t1.host_ns << 6) | (more ? 0x20 : 0x00));
        blk_len = ccid_build_t1_block(blk, pcb, apdu + off, chunk);

        err = t1_exchange_block(ctx, blk, blk_len, rblk, sizeof(rblk), &rblk_len);
        if (err != ESP_OK)
            return err;

        if (t1_is_r_block(rblk[1]) &&
            ((rblk[1] >> 4) & 0x01U) == ctx->t1.host_ns) {
            /* Card asks for our block again */
            if (++retries > 3)
                return ESP_ERR_INVALID_RESPONSE;
            continue;
        }
        ctx->t1.host_ns ^= 1;
        if (!more)
            break;
        if (!t1_is_r_block(rblk[1])) {
            ESP_LOGW(TAG, "T=1: expected R-block in chain, got PCB=0x%02X", rblk[1]);
            return ESP_ERR_INVALID_RESPONSE;
        }
        off += chunk;
        retries = 0;
    }

    /* Receive phase: collect I-blocks, acknowledging chained ones. */
    for (;;) {
        uint8_t rpcb = rblk[1];
        if (t1_is_i_block(rpcb)) {
            size_t inf_len = rblk[2];
            if (t1_i_ns(rpcb) != ctx->t1.expected_card_ns)
                ESP_LOGW(TAG, "T=1: unexpected card N(S)=%u", t1_i_ns(rpcb));
            ctx->t1.expected_card_ns = t1_i_ns(rpcb) ^ 1U;
            if (out_len + inf_len > response_cap)
                return ESP_ERR_NO_MEM;
            memcpy(response + out_len, &rblk[3], inf_len);
            out_len += inf_len;
            if ((rpcb & 0x20U) == 0)
                break;
            blk_len = ccid_build_t1_r_block(blk, ctx->t1.expected_card_ns);
        } else if ((rpcb & 0xC0U) == 0xC0U && (rpcb & 0x3FU) == 0x01U) {
            /* S(IFS request): accept the card's new IFSC */
            ifsc = rblk[3];
            blk_len = ccid_build_t1_block(blk, 0xE1, &rblk[3], 1);
        } else if (t1_is_r_block(rpcb)) {
            /* Card did not get our last block: blk still holds it */
            if (++retries > 3)
                return ESP_ERR_INVALID_RESPONSE;
        } else {
            ESP_LOGW(TAG, "T=1: unexpected block PCB=0x%02X", rpcb);
            return ESP_ERR_INVALID_RESPONSE;
        }

        err = t1_exchange_block(ctx, blk, blk_len, rblk, sizeof(rblk), &rblk_len);
        if (err != ESP_OK)
            return err;
    }

    if (out_len < 2)
        return ESP_ERR_INVALID_RESPONSE;
    if (response_len)
        *response_len = out_len;
    if (sw)
        *sw = ((uint16_t)response[out_len - 2] << 8) | response[out_len - 1];
    return ESP_OK;
}

/* Transmit APDU using TPDU mode (raw T=0) */
static esp_err_t transmit_apdu_tpdu(sec1210_ctx_t *ctx, const uint8_t *apdu,
                                     size_t apdu_len, uint8_t *response,
                                     size_t response_cap, size_t *response_len,
                                     uint16_t *sw) {
    uint8_t buf[SMARTCARD_SEC1210_CCID_MSG_MIN_LEN + SMARTCARD_SEC1210_APDU_MAX_LEN];
    uint8_t resp[SMARTCARD_SEC1210_RESPONSE_MAX_LEN];
    size_t resp_len;

    /* Send APDU directly via CCID XfrBlock */
    size_t cmd_len = build_xfr_block(buf, 0, ctx->seq++, apdu, apdu_len);
    esp_err_t err = sec1210_send_receive(ctx, buf, cmd_len, resp, sizeof(resp), &resp_len);
    if (err != ESP_OK) return err;

    return extract_apdu_response(resp, resp_len, response, response_cap,
                                  response_len, sw);
}

/* APDU identification steps */
static const uint8_t *apdu_for_step(size_t step, size_t *len, const char **name) {
    switch (step) {
    case 0:
        *len = sizeof(k_select_satochip_apdu);
        *name = "Satochip";
        return k_select_satochip_apdu;
    case 1:
        *len = sizeof(k_select_seedkeeper_apdu);
        *name = "SeedKeeper";
        return k_select_seedkeeper_apdu;
    case 2:
        *len = sizeof(k_select_mf_apdu);
        *name = "SELECT MF";
        return k_select_mf_apdu;
    case 3:
        *len = sizeof(k_get_status_apdu);
        *name = "GET STATUS";
        return k_get_status_apdu;
    default:
        *len = 0;
        *name = NULL;
        return NULL;
    }
}

/* Report helpers */
static const char *exchange_level_name(uint32_t features) {
    if ((features & SMARTCARD_SEC1210_CCID_FEATURE_SHORT_APDU_EXCHANGE) != 0)
        return "Short APDU";
    if ((features & SMARTCARD_SEC1210_CCID_FEATURE_TPDU_EXCHANGE) != 0)
        return "TPDU";
    return "Unknown";
}

static void report_update(sec1210_ctx_t *ctx) {
    ctx->report.ccid_protocols = ctx->dw_protocols;
    ctx->report.ccid_features = ctx->dw_features;
    ctx->report.t1_present = ctx->atr_t1_present;
}

/* Public API: start */
esp_err_t smartcard_sec1210_start(void) {
    sec1210_ctx_t *ctx = &s_ctx;

    if (ctx->initialized) return ESP_OK;

    uart_port_t uart_num = CONFIG_KSIG_SEC1210_UART_NUM;
    int tx_pin = CONFIG_KSIG_SEC1210_UART_TX_PIN;
    int rx_pin = CONFIG_KSIG_SEC1210_UART_RX_PIN;
    int baudrate = CONFIG_KSIG_SEC1210_UART_BAUDRATE;

    /* Install UART driver */
    esp_err_t err = uart_driver_install(uart_num, 4096, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Configure UART */
    uart_config_t uart_config = {
        .baud_rate = baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    err = uart_param_config(uart_num, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed: %s", esp_err_to_name(err));
        uart_driver_delete(uart_num);
        return err;
    }

    err = uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART pin set failed: %s", esp_err_to_name(err));
        uart_driver_delete(uart_num);
        return err;
    }

    /* Force the RX pad input-only. Routing a UART RX signal through the GPIO
     * matrix does not disable the pad's output driver, so this is a defensive
     * guard against the receive pin ever being shared with an output and
     * colliding with the reader. */
    err = gpio_set_direction((gpio_num_t)rx_pin, GPIO_MODE_INPUT);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not force RX pad input-only: %s",
                 esp_err_to_name(err));
    }

    /* Linux ccid_serial.c holds RTS low to inhibit the SEC1210 plug-and-play
     * string. RTS is left unmapped here: flow control is disabled and the hat
     * does not wire RTS, so there is nothing to drive. */

    /* Flush UART buffers */
    uart_flush(uart_num);

    ctx->initialized = true;

    /* SEC1210 init: ccid_serial.c sends an Escape carrying the single byte
     * 0x06 ("unknown but supported command") when opening a SEC1210, and
     * deliberately skips the GemPC card-movement escape. Best effort - a
     * reader that does not answer is not fatal here. */
    {
        uint8_t cmd[SMARTCARD_SEC1210_CCID_MSG_MIN_LEN + 1];
        uint8_t resp[SMARTCARD_SEC1210_RESPONSE_MAX_LEN];
        size_t resp_len = 0;
        const uint8_t payload = 0x06;
        size_t cmd_len = build_escape(cmd, 0, ctx->seq++, &payload, 1);
        esp_err_t esc =
            sec1210_send_receive(ctx, cmd, cmd_len, resp, sizeof(resp), &resp_len);
        ESP_LOGI(TAG, "SEC1210 init escape: %s (%u bytes)", esp_err_to_name(esc),
                 (unsigned)resp_len);
    }
    ctx->report.state = SMARTCARD_SEC1210_STATE_READY;
    ctx->report.initialized = true;
    snprintf(ctx->report.detail, sizeof(ctx->report.detail),
             "SEC1210 initialized on UART%d at %d baud (TX=%d RX=%d)",
             uart_num, baudrate, tx_pin, rx_pin);

    ESP_LOGI(TAG, "%s", ctx->report.detail);
    return ESP_OK;
}

/* Public API: probe */
esp_err_t smartcard_sec1210_probe(uint32_t timeout_ms) {
    sec1210_ctx_t *ctx = &s_ctx;
    uint8_t buf[64];
    uint8_t resp[SMARTCARD_SEC1210_RESPONSE_MAX_LEN];
    size_t resp_len;
    esp_err_t err;
    const char *applet_name = NULL;

    if (!ctx->initialized) {
        err = smartcard_sec1210_start();
        if (err != ESP_OK) return err;
    }

    ctx->report.state = SMARTCARD_SEC1210_STATE_STARTING;
    snprintf(ctx->report.detail, sizeof(ctx->report.detail), "%s",
             "Probing SEC1210...");

    /* Step 1: Power on the card at 5V, then 3V and 1.8V. The SEC1210 lacks
     * the auto-voltage and auto-activation features, so bVoltage=0x00 (auto)
     * fails with HW_ERROR; Linux CmdPowerOn also starts at 5V for it. The
     * reply's bStatus/bError say why there is no ATR, so log them. */
    static const uint8_t k_voltages[] = {0x01, 0x02, 0x03};
    uint8_t status = 0, error = 0;
    bool have_reply = false;
    size_t cmd_len;
    err = ESP_ERR_TIMEOUT;
    uart_flush_input(CONFIG_KSIG_SEC1210_UART_NUM);
    for (size_t v = 0; v < sizeof(k_voltages); v++) {
        ESP_LOGI(TAG, "Powering on card (bVoltage=0x%02X)...", k_voltages[v]);
        cmd_len = build_power_on(buf, 0, ctx->seq++, k_voltages[v]);
        err = sec1210_send_receive(ctx, buf, cmd_len, resp, sizeof(resp), &resp_len);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "PowerOn: no reply (%s)", esp_err_to_name(err));
            continue;
        }
        sec1210_log_frame("PowerOn reply", resp, resp_len);
        if (!parse_ccid_status(resp, resp_len, &status, &error)) {
            ESP_LOGW(TAG, "PowerOn: unexpected message type 0x%02X", resp[0]);
            err = ESP_ERR_INVALID_RESPONSE;
            continue;
        }
        have_reply = true;
        ESP_LOGI(TAG, "PowerOn: bStatus=0x%02X (ICC %s, command %s) bError=0x%02X (%s)",
                 status, ccid_icc_status_name(status),
                 (status & 0x40U) ? "failed" : "ok", error,
                 ccid_error_name(error));
        if ((status & 0x40U) == 0)
            break; /* ATR received */
        if ((status & 0x03U) == 0x02U)
            break; /* no card in the slot: another voltage will not help */
    }

    if (!have_reply) {
        ctx->report.state = SMARTCARD_SEC1210_STATE_TIMEOUT;
        snprintf(ctx->report.detail, sizeof(ctx->report.detail),
                 "Reader did not answer PowerOn (%s).", esp_err_to_name(err));
        return ESP_ERR_TIMEOUT;
    }

    if (status & 0x40U) {
        if ((status & 0x03U) == 0x02U) {
            ctx->report.card_present = false;
            ctx->report.state = SMARTCARD_SEC1210_STATE_WAITING;
            snprintf(ctx->report.detail, sizeof(ctx->report.detail),
                     "Reader reports no card in slot (bStatus=0x%02X).", status);
            return ESP_ERR_TIMEOUT;
        }
        ctx->report.card_present = true;
        ctx->report.state = SMARTCARD_SEC1210_STATE_FAIL;
        snprintf(ctx->report.detail, sizeof(ctx->report.detail),
                 "No ATR: %s (bStatus=0x%02X bError=0x%02X).",
                 ccid_error_name(error), status, error);
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* Extract ATR */
    size_t atr_len = extract_atr(resp, resp_len, ctx->report.atr,
                                  sizeof(ctx->report.atr));
    if (atr_len == 0) {
        ctx->report.state = SMARTCARD_SEC1210_STATE_FAIL;
        snprintf(ctx->report.detail, sizeof(ctx->report.detail), "%s",
                 "Failed to read ATR.");
        return ESP_ERR_INVALID_RESPONSE;
    }

    ctx->card_powered = true;
    ctx->report.card_present = true;
    ctx->t1.host_ns = 0;
    ctx->t1.expected_card_ns = 0;
    ctx->report.atr_len = atr_len;
    ESP_LOGI(TAG, "ATR received (%lu bytes):", (unsigned long)atr_len);
    for (size_t i = 0; i < atr_len && i < 16; i++) {
        ESP_LOGI(TAG, " %02X", ctx->report.atr[i]);
    }
    if (atr_len > 16) ESP_LOGI(TAG, " ... (%lu more)", (unsigned long)(atr_len - 16));

    /* Parse ATR for T=1 parameters */
    parse_atr_t_params(ctx, ctx->report.atr, atr_len);

    /* Set CCID descriptor from Linux ccid_serial.c SEC1210 config:
     * dwFeatures = 0x000100B2 (TPDU level, auto PPS, auto clock/baud, ATR
     * parameter config; no auto voltage, no short APDU level)
     * dwDefaultClock = 4800 Hz
     * dwMaxDataRate = 826000 bps */
    ctx->dw_features = 0x000100B2U;
    ctx->dw_default_clock = 4800;
    ctx->dw_max_data_rate = 826000;
    ctx->dw_protocols = SMARTCARD_SEC1210_CCID_PROTOCOL_T0 |
                        SMARTCARD_SEC1210_CCID_PROTOCOL_T1;
    ctx->dw_max_ifsd = 254;

    /* Determine exchange mode */
    ctx->tpdu_exchange = ((ctx->dw_features & 0x00010000U) != 0) &&
                         ((ctx->dw_features & SMARTCARD_SEC1210_CCID_FEATURE_SHORT_APDU_EXCHANGE) == 0);

    /* Step 2: Set T=1 parameters if card supports T=1 */
    if (ctx->atr_t1_present) {
        ESP_LOGI(TAG, "Setting T=1 parameters...");
        cmd_len = build_set_parameters(ctx, buf, 0, ctx->seq++);
        err = sec1210_send_receive(ctx, buf, cmd_len, resp, sizeof(resp), &resp_len);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "SetParameters: no reply (%s)", esp_err_to_name(err));
        } else if (parse_ccid_status(resp, resp_len, &status, &error) &&
                   (status & 0x40U)) {
            ESP_LOGW(TAG, "SetParameters failed: bStatus=0x%02X bError=0x%02X (%s)",
                     status, error, ccid_error_name(error));
        }
    }

    /* Step 3: Identify applet by trying SELECT sequences */
    ctx->report.state = SMARTCARD_SEC1210_STATE_ATR_OK;
    snprintf(ctx->report.detail, sizeof(ctx->report.detail), "%s",
             "ATR read. Identifying applet...");

    for (size_t step = 0; step < 4; step++) {
        size_t apdu_len = 0;
        const char *name = NULL;
        const uint8_t *apdu = apdu_for_step(step, &apdu_len, &name);
        if (!apdu) break;

        ESP_LOGI(TAG, "SELECT applet: %s", name);

        size_t tx_resp_len = 0;
        uint16_t tx_sw = 0;

        if (!ctx->atr_t1_present) {
            err = transmit_apdu_tpdu(ctx, apdu, apdu_len, resp, sizeof(resp),
                                      &tx_resp_len, &tx_sw);
        } else {
            err = transmit_apdu_t1(ctx, apdu, apdu_len, resp, sizeof(resp),
                                    &tx_resp_len, &tx_sw);
        }

        if (err == ESP_OK && tx_sw == SMARTCARD_SEC1210_CARD_SW_OK) {
            applet_name = name;
            ctx->report.state = SMARTCARD_SEC1210_STATE_APPLET_OK;
            snprintf(ctx->report.detail, sizeof(ctx->report.detail),
                     "Applet found: %s (SW=0x%04X)", name, tx_sw);
            break;
        }

        ESP_LOGI(TAG, "  %s: SW=0x%04X", name, tx_sw);
    }

    if (!applet_name) {
        /* Try GET STATUS as fallback */
        ESP_LOGI(TAG, "Trying GET STATUS...");
        size_t tx_resp_len = 0;
        uint16_t tx_sw = 0;

        if (!ctx->atr_t1_present) {
            err = transmit_apdu_tpdu(ctx, k_get_status_apdu, sizeof(k_get_status_apdu),
                                      resp, sizeof(resp), &tx_resp_len, &tx_sw);
        } else {
            err = transmit_apdu_t1(ctx, k_get_status_apdu, sizeof(k_get_status_apdu),
                                      resp, sizeof(resp), &tx_resp_len, &tx_sw);
        }

        if (err == ESP_OK) {
            applet_name = "Card Ready";
            ctx->report.state = SMARTCARD_SEC1210_STATE_APPLET_OK;
            snprintf(ctx->report.detail, sizeof(ctx->report.detail),
                     "Card ready (SW=0x%04X)", tx_sw);
        } else {
            ctx->report.state = SMARTCARD_SEC1210_STATE_FAIL;
            snprintf(ctx->report.detail, sizeof(ctx->report.detail),
                     "Applet identification failed: %s", esp_err_to_name(err));
        }
    }

    report_update(ctx);
    return err;
}

/* Public API: transmit APDU */
esp_err_t smartcard_sec1210_transmit_apdu(const uint8_t *apdu, size_t apdu_len,
                                           uint8_t *response, size_t response_cap,
                                           size_t *response_len, uint16_t *sw,
                                           uint32_t timeout_ms) {
    sec1210_ctx_t *ctx = &s_ctx;
    esp_err_t err;

    (void)timeout_ms; /* Uses internal timeouts per operation */

    if (!ctx->initialized || !ctx->card_powered)
        return ESP_ERR_INVALID_STATE;

    ctx->apdu_active = true;
    ctx->apdu_completed = false;
    ctx->apdu_resp = response;
    ctx->apdu_resp_cap = response_cap;
    ctx->apdu_resp_len = 0;
    ctx->apdu_sw = 0;

    if (!ctx->atr_t1_present) {
        err = transmit_apdu_tpdu(ctx, apdu, apdu_len, response, response_cap,
                                  response_len, sw);
    } else {
        err = transmit_apdu_t1(ctx, apdu, apdu_len, response, response_cap,
                                response_len, sw);
    }

    ctx->apdu_active = false;
    ctx->apdu_completed = true;

    if (response_len) ctx->apdu_resp_len = *response_len;
    if (sw) ctx->apdu_sw = *sw;
    ctx->apdu_result = err;

    return err;
}

/* Public API: snapshot */
void smartcard_sec1210_snapshot(smartcard_sec1210_report_t *out) {
    sec1210_ctx_t *ctx = &s_ctx;
    if (!out) return;

    memcpy(out, &ctx->report, sizeof(smartcard_sec1210_report_t));
}

/* Public API: state name */
const char *smartcard_sec1210_state_name(smartcard_sec1210_state_t state) {
    switch (state) {
    case SMARTCARD_SEC1210_STATE_IDLE: return "Idle";
    case SMARTCARD_SEC1210_STATE_STARTING: return "Starting";
    case SMARTCARD_SEC1210_STATE_WAITING: return "Waiting for card";
    case SMARTCARD_SEC1210_STATE_READY: return "Ready";
    case SMARTCARD_SEC1210_STATE_ATR_OK: return "ATR OK";
    case SMARTCARD_SEC1210_STATE_APPLET_OK: return "Applet identified";
    case SMARTCARD_SEC1210_STATE_PASS: return "Pass";
    case SMARTCARD_SEC1210_STATE_TIMEOUT: return "Timeout";
    case SMARTCARD_SEC1210_STATE_FAIL: return "Fail";
    default: return "Unknown";
    }
}

/* Public API: format report */
void smartcard_sec1210_format_report(char *out, size_t out_len) {
    if (!out || out_len == 0) return;

    sec1210_ctx_t *ctx = &s_ctx;
    /* Hex dump of the first bytes ever received, for bring-up: a zero RX count
     * means nothing is arriving on the wire at all (electrical/wiring), rather
     * than a protocol parsing failure. */
    char rx_hex[3 * sizeof(ctx->rx_first) + 1];
    size_t pos = 0;
    for (uint8_t i = 0; i < ctx->rx_first_len; i++) {
        int n = snprintf(rx_hex + pos, sizeof(rx_hex) - pos, "%02X ",
                         ctx->rx_first[i]);
        if (n <= 0 || (size_t)n >= sizeof(rx_hex) - pos)
            break;
        pos += (size_t)n;
    }
    rx_hex[pos] = '\0';

    snprintf(out, out_len,
             "SEC1210 UART Smart Card\n"
             "State: %s\n"
             "Initialized: %s\n"
             "Card present: %s\n"
             "Protocols: 0x%08" PRIx32 "\n"
             "Features: 0x%08" PRIx32 " (%s)\n"
             "T=1 supported: %s\n"
             "Max IFS: %" PRIu32 "\n"
             "UART: TX=%d RX=%d @%d\n"
             "RX bytes: %" PRIu32 "\n"
             "RX first: %s\n"
             "Detail: %s",
             smartcard_sec1210_state_name(ctx->report.state),
             ctx->report.initialized ? "yes" : "no",
             ctx->report.card_present ? "yes" : "no",
             ctx->dw_protocols,
             ctx->dw_features,
             exchange_level_name(ctx->dw_features),
             ctx->atr_t1_present ? "yes" : "no",
             ctx->dw_max_ifsd,
             CONFIG_KSIG_SEC1210_UART_TX_PIN, CONFIG_KSIG_SEC1210_UART_RX_PIN,
             CONFIG_KSIG_SEC1210_UART_BAUDRATE,
             ctx->rx_bytes_total,
             ctx->rx_first_len ? rx_hex : "(none)",
             ctx->report.detail);
}

#endif /* !SIMULATOR */