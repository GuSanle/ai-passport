#include "eevee_client.h"
#include "eevee_parser.h"
#include "bsp_audio.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "eevee_client";
#define HTTP_RESP_BUFFER_SIZE 1024

typedef struct {
    char *buf;
    size_t size;
    size_t len;
} http_resp_buffer_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_resp_buffer_t *resp = (http_resp_buffer_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (resp && resp->buf && resp->len + evt->data_len < resp->size) {
            memcpy(resp->buf + resp->len, evt->data, evt->data_len);
            resp->len += evt->data_len;
            resp->buf[resp->len] = '\0';
        }
    }
    return ESP_OK;
}

static bool http_request_sync(const eevee_config_t *cfg,
                             const char *path,
                             esp_http_client_method_t method,
                             const char *req_body,
                             char *out_resp, size_t out_resp_size)
{
    if (!cfg || !path || !out_resp || out_resp_size == 0) return false;
    out_resp[0] = '\0';

    char url[256];
    snprintf(url, sizeof(url), "%s%s", cfg->base_url, path);

    http_resp_buffer_t resp_ctx = {
        .buf = out_resp,
        .size = out_resp_size,
        .len = 0,
    };

    esp_http_client_config_t config = {
        .url = url,
        .method = method,
        .timeout_ms = 5000,
        .event_handler = http_event_handler,
        .user_data = &resp_ctx,
        .buffer_size = 512,
        .buffer_size_tx = 512,
        .disable_auto_redirect = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return false;
    }

    // 设置统一认证 Header (Bearer PAT)
    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", cfg->pat_token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Accept", "application/json");

    if (req_body && (method == HTTP_METHOD_POST || method == HTTP_METHOD_PUT)) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, req_body, strlen(req_body));
    }

    esp_err_t err = esp_http_client_perform(client);
    bool success = false;
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status >= 200 && status < 300) {
            ESP_LOGI(TAG, "HTTP %s %s -> %d (%zu bytes)",
                     (method == HTTP_METHOD_POST) ? "POST" : "GET", path, status, resp_ctx.len);
            success = true;
        } else {
            ESP_LOGW(TAG, "HTTP %s %s returned status %d",
                     (method == HTTP_METHOD_POST) ? "POST" : "GET", path, status);
        }
    } else {
        ESP_LOGE(TAG, "HTTP perform failed: %s (%s)", esp_err_to_name(err), url);
    }

    esp_http_client_cleanup(client);
    return success;
}

bool eevee_client_fetch_me(const eevee_config_t *cfg, eevee_profile_t *out_profile)
{
    char resp_buf[HTTP_RESP_BUFFER_SIZE];
    if (!http_request_sync(cfg, "/api/iot/me", HTTP_METHOD_GET, NULL,
                           resp_buf, sizeof(resp_buf))) {
        return false;
    }
    return eevee_parse_me(resp_buf, out_profile);
}

bool eevee_client_fetch_tasks(const eevee_config_t *cfg, int offset, eevee_task_t *out_task)
{
    char path[64];
    snprintf(path, sizeof(path), "/api/iot/tasks?offset=%d", offset);

    char resp_buf[HTTP_RESP_BUFFER_SIZE];
    if (!http_request_sync(cfg, path, HTTP_METHOD_GET, NULL,
                           resp_buf, sizeof(resp_buf))) {
        return false;
    }
    return eevee_parse_tasks(resp_buf, out_task);
}

bool eevee_client_execute_task_action(const eevee_config_t *cfg,
                                      const char *app_id,
                                      const char *record_id,
                                      const char *action_id,
                                      const char *comment)
{
    char body[256];
    if (!eevee_build_task_action_body(app_id, record_id, action_id, comment,
                                      body, sizeof(body))) {
        return false;
    }

    char resp_buf[256];
    return http_request_sync(cfg, "/api/iot/tasks/action", HTTP_METHOD_POST,
                             body, resp_buf, sizeof(resp_buf));
}

bool eevee_client_fetch_notifications(const eevee_config_t *cfg,
                                      eevee_notification_t *out_notif)
{
    char resp_buf[HTTP_RESP_BUFFER_SIZE];
    if (!http_request_sync(cfg, "/api/iot/notifications/unread", HTTP_METHOD_GET, NULL,
                           resp_buf, sizeof(resp_buf))) {
        return false;
    }
    return eevee_parse_notifications(resp_buf, out_notif);
}

bool eevee_client_mark_notifications_read(const eevee_config_t *cfg,
                                          const char *notif_id_or_all)
{
    char body[128];
    if (!eevee_build_mark_read_body(notif_id_or_all, body, sizeof(body))) {
        return false;
    }

    char resp_buf[128];
    return http_request_sync(cfg, "/api/iot/notifications/read", HTTP_METHOD_POST,
                             body, resp_buf, sizeof(resp_buf));
}

bool eevee_client_fetch_latest_record(const eevee_config_t *cfg,
                                      const char *app_id,
                                      eevee_record_t *out_record)
{
    if (!app_id || app_id[0] == '\0') return false;
    char path[128];
    snprintf(path, sizeof(path), "/api/iot/apps/%s/records/latest", app_id);

    char resp_buf[HTTP_RESP_BUFFER_SIZE];
    if (!http_request_sync(cfg, path, HTTP_METHOD_GET, NULL,
                           resp_buf, sizeof(resp_buf))) {
        return false;
    }
    return eevee_parse_latest_record(resp_buf, out_record);
}

bool eevee_client_report_record(const eevee_config_t *cfg,
                                const char *app_id,
                                float temp, float hum, float battery_volt)
{
    if (!app_id || app_id[0] == '\0') return false;
    char path[128];
    snprintf(path, sizeof(path), "/api/iot/apps/%s/records", app_id);

    char body[128];
    if (!eevee_build_record_body(temp, hum, battery_volt, body, sizeof(body))) {
        return false;
    }

    char resp_buf[256];
    return http_request_sync(cfg, path, HTTP_METHOD_POST,
                             body, resp_buf, sizeof(resp_buf));
}

bool eevee_client_create_record(const eevee_config_t *cfg,
                                const char *app_id,
                                const char *values_json,
                                int *out_record_number)
{
    if (!cfg || !app_id || app_id[0] == '\0' || !values_json) return false;

    char path[128];
    snprintf(path, sizeof(path), "/api/iot/apps/%s/records", app_id);

    char resp_buf[256];
    bool ok = http_request_sync(cfg, path, HTTP_METHOD_POST, values_json, resp_buf, sizeof(resp_buf));
    if (ok && out_record_number) {
        cJSON *root = cJSON_Parse(resp_buf);
        if (root) {
            cJSON *num = cJSON_GetObjectItem(root, "recordNumber");
            if (cJSON_IsNumber(num)) {
                *out_record_number = num->valueint;
            }
            cJSON_Delete(root);
        }
    }
    return ok;
}

static bool http_write_all(esp_http_client_handle_t client, const char *buf, int len)
{
    int ret = esp_http_client_write(client, buf, len);
    return (ret == len);
}

static bool send_http_chunk(esp_http_client_handle_t client, const void *data, size_t len)
{
    char hex_hdr[16];
    int hlen = snprintf(hex_hdr, sizeof(hex_hdr), "%X\r\n", (unsigned int)len);
    if (!http_write_all(client, hex_hdr, hlen)) return false;
    if (len > 0 && !http_write_all(client, (const char *)data, (int)len)) return false;
    if (!http_write_all(client, "\r\n", 2)) return false;
    return true;
}

/* A single task owns the client. Short waits allow cancellation without
 * closing/freeing an HTTP handle from the button task. */
bool eevee_client_parse_voice_stream(const eevee_config_t *cfg,
                                     const char *app_id,
                                     volatile bool *p_recording,
                                     volatile bool *p_abort,
                                     eevee_voice_progress_cb_t progress_cb,
                                     void *user_data,
                                     eevee_draft_t *out_draft)
{
    if (!cfg || !app_id || !app_id[0] || !p_recording || !p_abort || !out_draft) return false;
    memset(out_draft, 0, sizeof(*out_draft));
    if (*p_abort) return false;
    if (bsp_audio_init() != ESP_OK || bsp_audio_wake() != ESP_OK ||
        bsp_audio_set_format(16000, 16, 1) != ESP_OK) {
        snprintf(out_draft->summary, sizeof(out_draft->summary), "麦克风未就绪，请重试");
        return false;
    }

    char url[256];
    snprintf(url, sizeof(url), "%s/api/v1/apps/%s/record-drafts/parse", cfg->base_url, app_id);
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 3000,
        .buffer_size = 1024,
        .buffer_size_tx = 1024,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return false;
    bool success = false;
    char *resp_buf = NULL;
    const char *error = "语音请求失败，请重试";
    static const char boundary[] = "----EeveeVoiceBoundary";
    char content_type[96];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", boundary);
    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", cfg->pat_token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Content-Type", content_type);
    if (esp_http_client_open(client, -1) != ESP_OK || *p_abort) goto cleanup;
    esp_http_client_set_timeout_ms(client, 1000);

    char preamble[512];
    int preamble_len = snprintf(preamble, sizeof(preamble),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"metadata\"\r\n"
        "Content-Type: application/json\r\n\r\n"
        "{\"audio\":{\"format\":\"pcm_s16le\",\"sampleRate\":16000,\"channels\":1},\"timeZone\":\"Asia/Shanghai\"}\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"audio\"; filename=\"audio.pcm\"\r\n"
        "Content-Type: application/octet-stream\r\n\r\n", boundary, boundary);
    if (!send_http_chunk(client, preamble, (size_t)preamble_len)) goto cleanup;

    int16_t pcm_buf[512];
    size_t total_samples = 0;
    const size_t max_samples = 16000 * 20;
    const int64_t record_deadline = esp_timer_get_time() + 20LL * 1000000;
    int last_sec = -1;
    while (*p_recording && !*p_abort && total_samples < max_samples &&
           esp_timer_get_time() < record_deadline) {
        if (bsp_audio_read(pcm_buf, sizeof(pcm_buf)) != ESP_OK) {
            error = "录音失败，请重试";
            goto cleanup;
        }
        if (*p_abort) goto cleanup;
        if (!send_http_chunk(client, pcm_buf, sizeof(pcm_buf))) goto cleanup;
        total_samples += 512;
        int current_sec = (int)(total_samples / 16000);
        if (current_sec != last_sec) {
            last_sec = current_sec;
            if (progress_cb && *p_recording && !*p_abort) progress_cb(current_sec, user_data);
        }
    }
    *p_recording = false;
    if (*p_abort) goto cleanup;
    if (!total_samples) {
        error = "录音太短，请重试";
        goto cleanup;
    }

    char footer[64];
    int footer_len = snprintf(footer, sizeof(footer), "\r\n--%s--\r\n", boundary);
    if (!send_http_chunk(client, footer, (size_t)footer_len) ||
        !http_write_all(client, "0\r\n\r\n", 5)) goto cleanup;

    esp_http_client_set_timeout_ms(client, 500);
    const int64_t response_deadline = esp_timer_get_time() + 50LL * 1000000;
    int64_t content_len;
    int status = -1;
    do {
        if (*p_abort) goto cleanup;
        if (esp_timer_get_time() >= response_deadline) {
            error = "解析超时，请重试";
            goto cleanup;
        }
        content_len = esp_http_client_fetch_headers(client);
        /* IDF 5.5.3 clears status_code on each fetch_headers retry; preserve a
         * status line received before a timeout in the rest of the headers. */
        int observed = esp_http_client_get_status_code(client);
        if (observed > 0) status = observed;
    } while (content_len == -ESP_ERR_HTTP_EAGAIN);
    if (content_len < 0) goto cleanup;
    if (content_len >= HTTP_RESP_BUFFER_SIZE) {
        error = "返回内容过长，请简短录入";
        goto cleanup;
    }
    resp_buf = malloc(HTTP_RESP_BUFFER_SIZE);
    if (!resp_buf) {
        error = "内存不足，无法解析";
        goto cleanup;
    }
    int read_total = 0;
    for (;;) {
        if (*p_abort) goto cleanup;
        if (esp_timer_get_time() >= response_deadline) {
            error = "解析超时，请重试";
            goto cleanup;
        }
        /* Read one decoded byte at a time: the small response is bounded and
         * cached bytes are cheap; each transport wait remains cancellable. */
        char next;
        int r = esp_http_client_read(client, &next, 1);
        if (r == -ESP_ERR_HTTP_EAGAIN) continue;
        if (r < 0 || (r == 0 && !esp_http_client_is_complete_data_received(client))) goto cleanup;
        if (r == 0) break;
        if (read_total == HTTP_RESP_BUFFER_SIZE - 1) {
            error = "返回内容过长，请简短录入";
            goto cleanup;
        }
        resp_buf[read_total++] = next;
    }
    resp_buf[read_total] = '\0';
    bool parsed = eevee_parse_record_draft(resp_buf, out_draft);
    success = status >= 200 && status < 300 && parsed;
    ESP_LOGI(TAG, "Voice parse HTTP status %d, draft valid=%d", status, success);
    if (!success) out_draft->valid = false;

cleanup:
    *p_recording = false;
    if (!success && !out_draft->summary[0] && !*p_abort) {
        snprintf(out_draft->summary, sizeof(out_draft->summary), "%s", error);
    }
    free(resp_buf);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return success;
}
