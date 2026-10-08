#include "eevee_app.h"
#include "eevee_config.h"
#include "eevee_client.h"
#include "eevee_badge_ui.h"
#include "eevee_font.h"
#include "eevee_prov.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"

#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sleep.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "eevee_app";

#define WIFI_MAX_RETRIES 5
#define SCREEN_OFF_TIMEOUT_SEC 60

typedef enum {
    CMD_SYNC_ALL = 1,
    CMD_POLL_STATUS,
    CMD_TASK_OFFSET_CHANGE,
    CMD_TASK_APPROVE,
    CMD_TASK_REJECT,
    CMD_RECORD_REPORT,
} eevee_cmd_type_t;

typedef struct {
    eevee_cmd_type_t type;
    int int_val;
} eevee_cmd_t;

static eevee_config_t s_cfg;
static eevee_profile_t s_profile;
static eevee_task_t s_curr_task;
static eevee_record_t s_curr_record;
static eevee_draft_t s_curr_draft;
static volatile bool s_voice_recording = false;
static volatile bool s_voice_abort = false;
static TaskHandle_t s_voice_task_handle = NULL;

static volatile bool s_wifi_connected = false;
static volatile bool s_wifi_started = false;
static volatile bool s_screen_on = true;
static volatile bool s_is_sleeping = false;
static volatile bool s_just_woke_up = false;
static bool s_sntp_initialized = false;
static volatile bool s_time_synced = false;

static uint32_t s_idle_seconds = 0;
static int s_wifi_retry_count = 0;
static int s_curr_task_offset = 0;
static QueueHandle_t s_cmd_queue = NULL;
static TaskHandle_t s_worker_handle = NULL;
static TaskHandle_t s_console_handle = NULL;

static void eevee_app_enter_prov_mode(void);
static void eevee_app_exit_prov_mode(void);
static void eevee_app_enter_sleep(void);
static void eevee_app_wake_up(void);
static void eevee_time_init(void);
static bool eevee_time_get_str(char *buf, size_t max_len);
static bool eevee_time_get_date_str(char *buf, size_t max_len);

static void time_sync_notification_cb(struct timeval *tv)
{
    (void)tv;
    s_time_synced = true;
    ESP_LOGI(TAG, "SNTP network time synchronized successfully!");
}

static void eevee_time_init(void)
{
    if (s_sntp_initialized) {
        esp_sntp_restart();
        return;
    }
    ESP_LOGI(TAG, "Initializing SNTP client (CST-8)...");
    setenv("TZ", "CST-8", 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "pool.ntp.org");
    esp_sntp_setservername(2, "time.asia.apple.com");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
    s_sntp_initialized = true;
}

static bool eevee_time_get_str(char *buf, size_t max_len)
{
    if (!buf || max_len == 0) return false;
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2025 - 1900)) {
        snprintf(buf, max_len, "--:--");
        return false;
    }
    strftime(buf, max_len, "%H:%M", &timeinfo);
    return true;
}

static bool eevee_time_get_date_str(char *buf, size_t max_len)
{
    if (!buf || max_len == 0) return false;
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2025 - 1900)) {
        snprintf(buf, max_len, "待对时");
        return false;
    }
    strftime(buf, max_len, "%Y-%m-%d", &timeinfo);
    return true;
}

static void eevee_app_enter_sleep(void)
{
    if (!s_screen_on) return;
    s_screen_on = false;
    s_is_sleeping = true;
    ESP_LOGI(TAG, "Screen OFF -> Entering Stage 1+2 Low-power sleep...");

    if (eevee_badge_ui_is_voice_view_visible()) {
        s_voice_abort = true;
        s_voice_recording = false;
        eevee_badge_ui_show_voice_view(false);
    }

    // 1. 关闭屏幕背光
    bsp_display_backlight(0);

    // 2. 挂起 LVGL 渲染任务 (防止后台周期刷新打断 Light Sleep)
    bsp_lvgl_sleep();

    // 3. 让 ST7789 驱动芯片进入 Sleep In (降低显示驱动功耗)
    esp_lcd_panel_handle_t panel = bsp_display_panel();
    if (panel) {
        esp_lcd_panel_disp_on_off(panel, false);
        esp_lcd_panel_disp_sleep(panel, true);
    }

    // 4. 挂起音频芯片 ES8311 (切断偏置与内部时钟)
    bsp_audio_sleep();

    // 5. 挂起 Wi-Fi 射频天线 (切断天线功耗，Flash 中凭据保留)
    if (s_wifi_started) {
        s_wifi_connected = false;
        esp_wifi_disconnect();
        esp_wifi_stop();
        s_wifi_started = false;
        ESP_LOGI(TAG, "Wi-Fi radio stopped for low power.");
    }

    // 6. 挂起按键底层轮询定时器 (停止 5ms 高频 ADC 采样，防止打断 Light Sleep)
    bsp_button_sleep();
}

static void eevee_app_wake_up(void)
{
    if (s_screen_on) return;
    s_screen_on = true;
    s_is_sleeping = false;
    s_just_woke_up = true;
    s_idle_seconds = 0;
    ESP_LOGI(TAG, "Waking up from sleep mode (Screen ON)...");

    // 1. 恢复按键底层轮询定时器与采样
    bsp_button_wake();

    // 2. 恢复 LVGL 渲染任务
    bsp_lvgl_wake();

    // 3. 唤醒 ST7789 芯片并恢复背光
    esp_lcd_panel_handle_t panel = bsp_display_panel();
    if (panel) {
        esp_lcd_panel_disp_sleep(panel, false);
        esp_lcd_panel_disp_on_off(panel, true);
    }
    if (bsp_lvgl_lock(250)) {
        // 立即刷新一次当前时间与电量
        char time_str[16];
        char date_str[32];
        eevee_time_get_str(time_str, sizeof(time_str));
        eevee_time_get_date_str(date_str, sizeof(date_str));
        eevee_badge_ui_update_time(time_str, date_str);
        eevee_badge_ui_update_status(false, bsp_battery_soc());
        bsp_lvgl_unlock();
    }
    bsp_display_backlight(80);

    // 4. 恢复音频芯片
    bsp_audio_wake();

    // 5. 重新拉起 Wi-Fi (后台静默自动连接，无感恢复)
    if (eevee_config_has_wifi(&s_cfg)) {
        ESP_LOGI(TAG, "Restarting Wi-Fi in background...");
        s_wifi_started = true;
        s_wifi_retry_count = 0;
        esp_wifi_start();
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            s_wifi_started = true;
            if (eevee_config_has_wifi(&s_cfg)) {
                ESP_LOGI(TAG, "Wi-Fi started, connecting to '%s'...", s_cfg.wifi_ssid);
                s_wifi_retry_count = 0;
                esp_wifi_connect();
            } else {
                ESP_LOGI(TAG, "Wi-Fi credentials not configured. Device running in offline badge mode.");
                ESP_LOGI(TAG, "Type 'wifi <SSID> <PASSWORD>' in serial console to connect.");
            }
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            s_wifi_connected = false;
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_update_status(false, bsp_battery_soc());
                eevee_badge_ui_update_settings(s_cfg.wifi_ssid, false, s_cfg.base_url, s_cfg.pat_token[0] != '\0');
                bsp_lvgl_unlock();
            }
            if (s_is_sleeping) {
                // 休眠期间主动断网，不触发自动重连重试
                return;
            }
            if (eevee_config_has_wifi(&s_cfg) && s_wifi_retry_count < WIFI_MAX_RETRIES) {
                s_wifi_retry_count++;
                ESP_LOGW(TAG, "Wi-Fi disconnected, reconnecting (%d/%d)...",
                         s_wifi_retry_count, WIFI_MAX_RETRIES);
                vTaskDelay(pdMS_TO_TICKS(1500));
                esp_wifi_connect();
            } else {
                ESP_LOGW(TAG, "Wi-Fi offline. Press OK on badge to retry, or use console.");
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Wi-Fi Connected! IP: " IPSTR, IP2STR(&evt->ip_info.ip));
        s_wifi_connected = true;
        s_wifi_retry_count = 0;

        // 初始化或触发 SNTP 网络对时
        eevee_time_init();

        if (bsp_lvgl_lock(250)) {
            char time_str[16];
            char date_str[32];
            eevee_time_get_str(time_str, sizeof(time_str));
            eevee_time_get_date_str(date_str, sizeof(date_str));
            eevee_badge_ui_update_time(time_str, date_str);
            eevee_badge_ui_update_status(true, bsp_battery_soc());
            eevee_badge_ui_update_settings(s_cfg.wifi_ssid, true, s_cfg.base_url, s_cfg.pat_token[0] != '\0');
            eevee_badge_ui_show_toast("网络已连接", 0x10B981);
            bsp_lvgl_unlock();
        }
        // 连上网立即触发全量同步
        eevee_cmd_t cmd = { .type = CMD_SYNC_ALL };
        xQueueSend(s_cmd_queue, &cmd, 0);
    }
}

static void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&init_cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                         &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                         &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_cfg;
    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
    strncpy((char *)wifi_cfg.sta.ssid, s_cfg.wifi_ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, s_cfg.wifi_pass, sizeof(wifi_cfg.sta.password) - 1);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    s_wifi_started = true;
    esp_wifi_start();
}

static void do_sync_all(void)
{
    if (!s_wifi_connected) return;
    ESP_LOGI(TAG, "Starting full sync from %s...", s_cfg.base_url);

    // 1. 同步个人卡面资料与名片地址 (/settings/profile)
    if (eevee_client_fetch_me(&s_cfg, &s_profile)) {
        if (bsp_lvgl_lock(250)) {
            eevee_badge_ui_update_profile(&s_profile);
            char web_url[128];
            eevee_config_get_web_url(&s_cfg, web_url, sizeof(web_url));
            char user_card_url[256];
            snprintf(user_card_url, sizeof(user_card_url), "%s/settings/profile", web_url);
            eevee_badge_ui_update_qr_url(user_card_url);
            bsp_lvgl_unlock();
        }
    }

    // 2. 同步待处理工作流
    if (eevee_client_fetch_tasks(&s_cfg, s_curr_task_offset, &s_curr_task)) {
        if (bsp_lvgl_lock(250)) {
            eevee_badge_ui_update_task(&s_curr_task);
            bsp_lvgl_unlock();
        }
    }

    // 3. 同步业务记录
    if (eevee_client_fetch_latest_record(&s_cfg, s_cfg.app_id, &s_curr_record)) {
        if (bsp_lvgl_lock(250)) {
            eevee_badge_ui_update_record(&s_curr_record);
            bsp_lvgl_unlock();
        }
    }

    if (bsp_lvgl_lock(250)) {
        eevee_badge_ui_update_status(s_wifi_connected, bsp_battery_soc());
        eevee_badge_ui_update_settings(s_cfg.wifi_ssid, s_wifi_connected, s_cfg.base_url, s_cfg.pat_token[0] != '\0');
        bsp_lvgl_unlock();
    }
}

static void do_poll_status(void)
{
    if (!s_wifi_connected) return;

    // 仅轮询待处理工作流程
    if (eevee_client_fetch_tasks(&s_cfg, s_curr_task_offset, &s_curr_task)) {
        if (bsp_lvgl_lock(250)) {
            eevee_badge_ui_update_task(&s_curr_task);
            eevee_badge_ui_update_status(s_wifi_connected, bsp_battery_soc());
            bsp_lvgl_unlock();
        }
    }
}

static void voice_progress_cb(int elapsed_sec, void *user_data)
{
    (void)user_data;
    if (bsp_lvgl_lock(100)) {
        eevee_badge_ui_update_voice_recording(elapsed_sec, 20);
        bsp_lvgl_unlock();
    }
}

static void eevee_voice_task(void *pvParam)
{
    (void)pvParam;
    ESP_LOGI(TAG, "eevee_voice_task started");

    eevee_draft_t draft;
    memset(&draft, 0, sizeof(draft));

    bool ok = eevee_client_parse_voice_stream(&s_cfg, s_cfg.app_id,
                                              &s_voice_recording,
                                              &s_voice_abort,
                                              voice_progress_cb,
                                              NULL,
                                              &draft);

    if (s_voice_abort) {
        ESP_LOGI(TAG, "Voice task ended by user abort");
        if (bsp_lvgl_lock(250)) {
            eevee_badge_ui_show_voice_view(false);
            eevee_badge_ui_show_toast("已取消录音", 0x64748B);
            bsp_lvgl_unlock();
        }
    } else if (ok && draft.valid) {
        memcpy(&s_curr_draft, &draft, sizeof(s_curr_draft));
        if (bsp_lvgl_lock(250)) {
            eevee_badge_ui_update_voice_review(draft.summary);
            bsp_lvgl_unlock();
        }
    } else {
        const char *err_msg = (draft.summary[0] != '\0') ? draft.summary : "语音解析失败";
        if (bsp_lvgl_lock(250)) {
            eevee_badge_ui_show_voice_view(false);
            eevee_badge_ui_show_toast(err_msg, 0xEF4444);
            bsp_lvgl_unlock();
        }
    }

    s_voice_recording = false;
    s_voice_task_handle = NULL;
    vTaskDelete(NULL);
}

static void eevee_worker_task(void *pvParam)
{
    (void)pvParam;
    int tick_count = 0;

    for (;;) {
        // 如果处于息屏休眠状态，执行 Stage 2 自动 Light-Sleep
        if (!s_screen_on) {
            // 1. 配置 GPIO0 低电平作为硬件唤醒源 (任意物理按键按下即拉低到 <0.6V)
            gpio_config_t io_conf = {
                .pin_bit_mask = (1ULL << GPIO_NUM_0),
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE, // 外部硬件已有 10k 上拉电阻
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_LOW_LEVEL,
            };
            gpio_config(&io_conf);
            gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);
            esp_sleep_enable_gpio_wakeup();

            // 2. 配置 60 秒定时器周期唤醒 (防止异常卡死并维持心跳)
            esp_sleep_enable_timer_wakeup(60ULL * 1000 * 1000);

            // 3. 进入 Light Sleep (CPU 暂停，SRAM 保持，整机功耗降至 ~1.5mA)
            esp_light_sleep_start();

            // 4. 醒来后立即禁用唤醒源
            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
            gpio_wakeup_disable(GPIO_NUM_0);

            esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
            if (cause == ESP_SLEEP_WAKEUP_GPIO) {
                ESP_LOGI(TAG, "Light sleep wake by GPIO0 (button press)!");
                eevee_app_wake_up();
            }
            continue;
        }

        eevee_cmd_t cmd;
        if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(1000)) == pdPASS) {
            switch (cmd.type) {
            case CMD_SYNC_ALL:
                do_sync_all();
                break;
            case CMD_POLL_STATUS:
                do_poll_status();
                break;
            case CMD_TASK_OFFSET_CHANGE:
                s_curr_task_offset = cmd.int_val;
                if (s_wifi_connected && eevee_client_fetch_tasks(&s_cfg, s_curr_task_offset, &s_curr_task)) {
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_update_task(&s_curr_task);
                        bsp_lvgl_unlock();
                    }
                }
                break;
            case CMD_TASK_APPROVE:
                if (!s_wifi_connected) {
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_toast("离线无法审批", 0xEF4444);
                        bsp_lvgl_unlock();
                    }
                    break;
                }
                if (s_curr_task.has_task && s_curr_task.primary_action_id[0] != '\0') {
                    bool ok = eevee_client_execute_task_action(
                        &s_cfg, s_curr_task.app_id, s_curr_task.record_id,
                        s_curr_task.primary_action_id, "工牌物理按键同意");
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_toast(ok ? "已同意审批" : "审批失败",
                                                  ok ? 0x10B981 : 0xEF4444);
                        bsp_lvgl_unlock();
                    }
                    if (ok) {
                        vTaskDelay(pdMS_TO_TICKS(500));
                        do_poll_status();
                    }
                }
                break;
            case CMD_TASK_REJECT:
                if (!s_wifi_connected) {
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_toast("离线无法驳回", 0xEF4444);
                        bsp_lvgl_unlock();
                    }
                    break;
                }
                if (s_curr_task.has_task) {
                    const char *action = s_curr_task.reject_action_id[0] ?
                                         s_curr_task.reject_action_id : "act_reject";
                    bool ok = eevee_client_execute_task_action(
                        &s_cfg, s_curr_task.app_id, s_curr_task.record_id,
                        action, "工牌物理按键驳回");
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_toast(ok ? "已驳回修改" : "驳回失败",
                                                  ok ? 0xF59E0B : 0xEF4444);
                        bsp_lvgl_unlock();
                    }
                    if (ok) {
                        vTaskDelay(pdMS_TO_TICKS(500));
                        do_poll_status();
                    }
                }
                break;
            case CMD_RECORD_REPORT: {
                if (!s_wifi_connected) {
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_toast("离线无法打卡", 0xEF4444);
                        bsp_lvgl_unlock();
                    }
                    break;
                }
                bool ok = false;
                int rec_num = -1;
                if (s_curr_draft.valid && s_curr_draft.raw_values_json[0] != '\0') {
                    ok = eevee_client_create_record(&s_cfg, s_cfg.app_id, s_curr_draft.raw_values_json, &rec_num);
                    memset(&s_curr_draft, 0, sizeof(s_curr_draft));
                } else {
                    int mv = bsp_battery_mv();
                    float volt = (mv > 2500 && mv < 4500) ? (mv / 1000.0f) : 3.84f;
                    ok = eevee_client_report_record(&s_cfg, s_cfg.app_id, 26.0f, 60.0f, volt);
                }

                if (bsp_lvgl_lock(250)) {
                    eevee_badge_ui_show_toast(ok ? "打卡上报成功" : "上报失败",
                                              ok ? 0x10B981 : 0xEF4444);
                    bsp_lvgl_unlock();
                }
                if (ok) {
                    vTaskDelay(pdMS_TO_TICKS(500));
                    if (eevee_client_fetch_latest_record(&s_cfg, s_cfg.app_id, &s_curr_record)) {
                        if (bsp_lvgl_lock(250)) {
                            eevee_badge_ui_update_record(&s_curr_record);
                            bsp_lvgl_unlock();
                        }
                    }
                }
                break;
            }
            default:
                break;
            }
        }

        // 仅在亮屏且非配网/非语音模态时计算超时息屏
        if (s_screen_on && !eevee_badge_ui_is_prov_view_visible() && !eevee_badge_ui_is_voice_view_visible()) {
            s_idle_seconds++;
            if (s_idle_seconds >= SCREEN_OFF_TIMEOUT_SEC) {
                eevee_app_enter_sleep();
                continue;
            }
        }

        // 仅在亮屏状态下更新时间与电量状态栏，并周期轮询待办流程
        if (s_screen_on) {
            char time_str[16];
            char date_str[32];
            eevee_time_get_str(time_str, sizeof(time_str));
            eevee_time_get_date_str(date_str, sizeof(date_str));

            if (bsp_lvgl_lock(100)) {
                eevee_badge_ui_update_time(time_str, date_str);
                eevee_badge_ui_update_status(s_wifi_connected, bsp_battery_soc());
                bsp_lvgl_unlock();
            }

            tick_count++;
            if (tick_count >= s_cfg.poll_interval_sec) {
                tick_count = 0;
                do_poll_status();
            }
        }
    }
}

static void eevee_console_task(void *pvParam)
{
    (void)pvParam;
    char line[256];

    printf("\n");
    printf("===================================================\n");
    printf("   Eevee Smart Badge Console Ready!                \n");
    printf("   Commands available:                             \n");
    printf("     wifi <SSID> <PASSWORD>                        \n");
    printf("     server <URL> [PAT_TOKEN]                      \n");
    printf("     sync                                          \n");
    printf("     status                                        \n");
    printf("===================================================\n\n");

    while (1) {
        if (fgets(line, sizeof(line), stdin)) {
            char *p = line;
            while (*p) {
                if (*p == '\r' || *p == '\n') *p = '\0';
                p++;
            }
            if (line[0] == '\0') continue;

            if (strncmp(line, "wifi ", 5) == 0) {
                char ssid[33] = {0};
                char pass[65] = {0};
                int cnt = sscanf(line + 5, "%32s %64s", ssid, pass);
                if (cnt >= 1) {
                    strncpy(s_cfg.wifi_ssid, ssid, sizeof(s_cfg.wifi_ssid) - 1);
                    if (cnt >= 2) {
                        strncpy(s_cfg.wifi_pass, pass, sizeof(s_cfg.wifi_pass) - 1);
                    } else {
                        s_cfg.wifi_pass[0] = '\0';
                    }
                    eevee_config_save(&s_cfg);
                    printf("[CONSOLE] Wi-Fi credentials saved: SSID='%s'. Connecting...\n", s_cfg.wifi_ssid);

                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_toast("配置已更新,连接中", 0x2563EB);
                        eevee_badge_ui_update_settings(s_cfg.wifi_ssid, false, s_cfg.base_url, s_cfg.pat_token[0] != '\0');
                        bsp_lvgl_unlock();
                    }

                    wifi_config_t wifi_cfg;
                    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
                    strncpy((char *)wifi_cfg.sta.ssid, s_cfg.wifi_ssid, sizeof(wifi_cfg.sta.ssid) - 1);
                    strncpy((char *)wifi_cfg.sta.password, s_cfg.wifi_pass, sizeof(wifi_cfg.sta.password) - 1);
                    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
                    s_wifi_retry_count = 0;
                    esp_wifi_disconnect();
                    esp_wifi_connect();
                }
            } else if (strncmp(line, "server ", 7) == 0) {
                char url[128] = {0};
                char pat[64] = {0};
                int parsed = sscanf(line + 7, "%127s %63s", url, pat);
                if (parsed >= 1) {
                    strncpy(s_cfg.base_url, url, sizeof(s_cfg.base_url) - 1);
                    if (parsed >= 2) {
                        strncpy(s_cfg.pat_token, pat, sizeof(s_cfg.pat_token) - 1);
                    }
                    eevee_config_save(&s_cfg);
                    printf("[CONSOLE] Server saved: BaseURL='%s', PAT='%s'\n", s_cfg.base_url, s_cfg.pat_token);
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_toast("服务端地址已更新", 0x2563EB);
                        eevee_badge_ui_update_settings(s_cfg.wifi_ssid, s_wifi_connected, s_cfg.base_url, s_cfg.pat_token[0] != '\0');
                        bsp_lvgl_unlock();
                    }
                    if (s_wifi_connected) {
                        eevee_cmd_t cmd = { .type = CMD_SYNC_ALL };
                        xQueueSend(s_cmd_queue, &cmd, 0);
                    }
                }
            } else if (strcmp(line, "sync") == 0) {
                printf("[CONSOLE] Requesting full sync...\n");
                if (s_wifi_connected) {
                    eevee_cmd_t cmd = { .type = CMD_SYNC_ALL };
                    xQueueSend(s_cmd_queue, &cmd, 0);
                } else {
                    printf("[CONSOLE] Error: Wi-Fi not connected, cannot sync.\n");
                }
            } else if (strcmp(line, "prov") == 0) {
                printf("[CONSOLE] Starting SoftAP Web Provisioning...\n");
                eevee_app_enter_prov_mode();
            } else if (strcmp(line, "status") == 0) {
                printf("[CONSOLE] === Eevee Badge Status ===\n");
                printf("  Wi-Fi SSID:    %s\n", s_cfg.wifi_ssid[0] ? s_cfg.wifi_ssid : "<NOT SET>");
                printf("  Wi-Fi Status:  %s\n", s_wifi_connected ? "ONLINE" : "OFFLINE");
                printf("  Server URL:    %s\n", s_cfg.base_url);
                printf("  PAT Token:     %s\n", s_cfg.pat_token[0] ? s_cfg.pat_token : "<NOT SET>");
                printf("  Free Heap:     %lu bytes\n", (unsigned long)esp_get_free_heap_size());
                printf("  Battery SOC:   %d%%\n", bsp_battery_soc());
                printf("===================================\n");
            } else if (strcmp(line, "screen off") == 0) {
                eevee_app_enter_sleep();
                printf("[CONSOLE] Screen turned OFF and entered low-power sleep mode.\n");
            } else if (strcmp(line, "screen on") == 0) {
                eevee_app_wake_up();
                printf("[CONSOLE] Screen turned ON. Woke up from sleep.\n");
            } else if (strcmp(line, "help") == 0) {
                printf("[CONSOLE] Available commands:\n");
                printf("  wifi <ssid> <pass>   - Set Wi-Fi and connect\n");
                printf("  server <url> [token] - Set Base URL and PAT Token\n");
                printf("  prov                 - Start SoftAP Web Provisioning\n");
                printf("  sync                 - Trigger full sync\n");
                printf("  screen on/off        - Wake up or turn off display\n");
                printf("  status               - Print current status and memory\n");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void on_prov_success(const eevee_config_t *new_cfg, bool success)
{
    if (success && new_cfg) {
        s_cfg = *new_cfg;
        ESP_LOGI(TAG, "Provisioning succeeded with SSID='%s'", s_cfg.wifi_ssid);
        if (bsp_lvgl_lock(250)) {
            eevee_badge_ui_show_prov_view(false, NULL, NULL);
            eevee_badge_ui_show_toast("配网成功,已连接", 0x10B981);
            eevee_badge_ui_update_settings(s_cfg.wifi_ssid, true, s_cfg.base_url, s_cfg.pat_token[0] != '\0');
            bsp_lvgl_unlock();
        }
        eevee_cmd_t cmd = { .type = CMD_SYNC_ALL };
        xQueueSend(s_cmd_queue, &cmd, 0);
    }
}

static void eevee_app_enter_prov_mode(void)
{
    ESP_LOGI(TAG, "Entering SoftAP Web Provisioning mode...");
    eevee_prov_start(&s_cfg, on_prov_success);
    if (bsp_lvgl_lock(250)) {
        eevee_badge_ui_show_prov_view(true, "Eevee-Badge-Setup", "192.168.4.1");
        bsp_lvgl_unlock();
    }
}

static void eevee_app_exit_prov_mode(void)
{
    ESP_LOGI(TAG, "Exiting Provisioning mode...");
    eevee_prov_stop();
    eevee_config_init(&s_cfg);
    if (bsp_lvgl_lock(250)) {
        eevee_badge_ui_show_prov_view(false, NULL, NULL);
        eevee_badge_ui_show_toast("已退出配网模式", 0x64748B);
        bsp_lvgl_unlock();
    }
    if (eevee_config_has_wifi(&s_cfg) && !s_wifi_connected) {
        wifi_config_t wifi_cfg;
        memset(&wifi_cfg, 0, sizeof(wifi_cfg));
        strncpy((char *)wifi_cfg.sta.ssid, s_cfg.wifi_ssid, sizeof(wifi_cfg.sta.ssid) - 1);
        strncpy((char *)wifi_cfg.sta.password, s_cfg.wifi_pass, sizeof(wifi_cfg.sta.password) - 1);
        esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
        s_wifi_retry_count = 0;
        esp_wifi_connect();
    }
}

void eevee_app_start(void)
{
    ESP_LOGI(TAG, "Starting Eevee Smart Badge Application...");

    if (bsp_audio_init() != ESP_OK) {
        ESP_LOGW(TAG, "Audio unavailable; voice entry will retry initialization");
    }
    eevee_config_init(&s_cfg);
    if (bsp_lvgl_lock(1000)) {
        eevee_font_init();
        eevee_badge_ui_init();
        char web_url[128];
        eevee_config_get_web_url(&s_cfg, web_url, sizeof(web_url));
        char user_card_url[256];
        snprintf(user_card_url, sizeof(user_card_url), "%s/settings/profile", web_url);
        eevee_badge_ui_update_qr_url(user_card_url);
        char time_str[16];
        char date_str[32];
        eevee_time_get_str(time_str, sizeof(time_str));
        eevee_time_get_date_str(date_str, sizeof(date_str));
        eevee_badge_ui_update_time(time_str, date_str);
        eevee_badge_ui_update_status(false, bsp_battery_soc());
        eevee_badge_ui_update_settings(s_cfg.wifi_ssid, false, s_cfg.base_url, s_cfg.pat_token[0] != '\0');
        bsp_lvgl_unlock();
    }

    s_cmd_queue = xQueueCreate(10, sizeof(eevee_cmd_t));
    xTaskCreate(eevee_worker_task, "eevee_worker", 8192, NULL, 5, &s_worker_handle);
    xTaskCreate(eevee_console_task, "eevee_console", 3072, NULL, 4, &s_console_handle);

    wifi_init_sta();

    if (!eevee_config_has_wifi(&s_cfg)) {
        ESP_LOGI(TAG, "No Wi-Fi configured. Entering SoftAP Web Provisioning mode...");
        eevee_app_enter_prov_mode();
    }
}

void eevee_app_handle_button(bsp_btn_t btn, bsp_btn_ev_t event)
{
    // 0. 息屏唤醒：如果处于息屏状态，任意键短按或长按均唤醒屏幕并恢复低功耗外设
    if (!s_screen_on) {
        if (event == BSP_BTN_CLICK || event == BSP_BTN_LONG) {
            eevee_app_wake_up();
        }
        return; // 唤醒动作拦截本次输入，防止黑暗中误触
    }

    if (s_just_woke_up) {
        s_just_woke_up = false;
        return; // 拦截硬件唤醒伴随的初次按键输入
    }

    // 活跃输入：重置无操作空闲计时器
    s_idle_seconds = 0;

    // 配网视图处于前台时，短按或长按 OK 都能退出配网
    if (eevee_badge_ui_is_prov_view_visible()) {
        if (btn == BSP_BTN_OK && (event == BSP_BTN_CLICK || event == BSP_BTN_LONG)) {
            eevee_app_exit_prov_mode();
        }
        return;
    }

    // 语音模态处于前台时，优先处理语音录入与草稿确认交互
    if (eevee_badge_ui_is_voice_view_visible()) {
        eevee_voice_ui_state_t vstate = eevee_badge_ui_get_voice_state();
        if (event == BSP_BTN_CLICK) {
            if (vstate == EEVEE_VOICE_UI_RECORDING) {
                if (btn == BSP_BTN_OK) {
                    // 短按 OK：主动结束录音并送去解析
                    ESP_LOGI(TAG, "User clicked OK to finish voice recording");
                    s_voice_recording = false;
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_update_voice_parsing();
                        bsp_lvgl_unlock();
                    }
                } else if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
                    // 短按 UP/DOWN：放弃录音并直接丢弃
                    ESP_LOGI(TAG, "User clicked UP/DOWN to abort voice recording");
                    s_voice_abort = true;
                    s_voice_recording = false;
                }
            } else if (vstate == EEVEE_VOICE_UI_PARSING) {
                if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
                    // 解析中途放弃
                    ESP_LOGI(TAG, "User cancelled waiting during voice parse");
                    s_voice_abort = true;
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_voice_view(false);
                        eevee_badge_ui_show_toast("已取消等待", 0x64748B);
                        bsp_lvgl_unlock();
                    }
                }
            } else if (vstate == EEVEE_VOICE_UI_REVIEW) {
                if (btn == BSP_BTN_OK) {
                    // 短按 OK：确认提交打卡草稿
                    ESP_LOGI(TAG, "User clicked OK to confirm draft report");
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_voice_view(false);
                        eevee_badge_ui_show_toast("正在提交打卡...", 0x2563EB);
                        bsp_lvgl_unlock();
                    }
                    eevee_cmd_t cmd = { .type = CMD_RECORD_REPORT };
                    xQueueSend(s_cmd_queue, &cmd, 0);
                } else if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
                    // 短按 UP/DOWN：取消丢弃草稿
                    ESP_LOGI(TAG, "User clicked UP/DOWN to discard draft");
                    memset(&s_curr_draft, 0, sizeof(s_curr_draft));
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_voice_view(false);
                        eevee_badge_ui_show_toast("已取消打卡", 0x64748B);
                        bsp_lvgl_unlock();
                    }
                }
            }
        }
        return;
    }

    eevee_ui_page_t page = eevee_badge_ui_get_page();

    // 1. 长按事件处理
    if (event == BSP_BTN_LONG) {
        if (btn == BSP_BTN_UP) {
            // 任意页面长按 UP：手动立即进入低功耗休眠
            eevee_app_enter_sleep();
            return;
        } else if (btn == BSP_BTN_OK) {
            if (page == EEVEE_PAGE_TASKS) {
                // 待办页面长按 OK：执行驳回
                eevee_cmd_t cmd = { .type = CMD_TASK_REJECT };
                xQueueSend(s_cmd_queue, &cmd, 0);
                return;
            } else if (page == EEVEE_PAGE_BADGE) {
                // 工牌主屏长按 OK：触发全量刷新同步
                if (bsp_lvgl_lock(250)) {
                    eevee_badge_ui_show_toast("正在全量同步...", 0x2563EB);
                    bsp_lvgl_unlock();
                }
                eevee_cmd_t cmd = { .type = CMD_SYNC_ALL };
                xQueueSend(s_cmd_queue, &cmd, 0);
                return;
            }
        } else if (btn == BSP_BTN_DOWN && page == EEVEE_PAGE_BADGE) {
            // 工牌主屏长按 DOWN：快捷启动热点配网模式
            eevee_app_enter_prov_mode();
            return;
        }
        return;
    }

    // 2. 短按事件处理
    if (event != BSP_BTN_CLICK) return;

    if (page == EEVEE_PAGE_BADGE) {
        if (btn == BSP_BTN_DOWN) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_TASKS);
                bsp_lvgl_unlock();
            }
        } else if (btn == BSP_BTN_UP) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_SETTINGS);
                bsp_lvgl_unlock();
            }
        } else if (btn == BSP_BTN_OK) {
            if (s_wifi_connected) {
                if (bsp_lvgl_lock(250)) {
                    eevee_badge_ui_show_toast("正在同步...", 0x2563EB);
                    bsp_lvgl_unlock();
                }
                eevee_cmd_t cmd = { .type = CMD_SYNC_ALL };
                xQueueSend(s_cmd_queue, &cmd, 0);
            } else if (eevee_config_has_wifi(&s_cfg)) {
                if (bsp_lvgl_lock(250)) {
                    eevee_badge_ui_show_toast("正在重连网络...", 0x2563EB);
                    bsp_lvgl_unlock();
                }
                s_wifi_retry_count = 0;
                esp_wifi_connect();
            } else {
                // 未配网时按 OK 自动拉起配网
                eevee_app_enter_prov_mode();
            }
        }
    } else if (page == EEVEE_PAGE_TASKS) {
        if (btn == BSP_BTN_OK) {
            // 短按 OK：执行同意
            eevee_cmd_t cmd = { .type = CMD_TASK_APPROVE };
            xQueueSend(s_cmd_queue, &cmd, 0);
        } else if (btn == BSP_BTN_UP) {
            if (s_curr_task_offset > 0) {
                eevee_cmd_t cmd = {
                    .type = CMD_TASK_OFFSET_CHANGE,
                    .int_val = s_curr_task_offset - 1
                };
                xQueueSend(s_cmd_queue, &cmd, 0);
            } else {
                if (bsp_lvgl_lock(250)) {
                    eevee_badge_ui_set_page(EEVEE_PAGE_BADGE);
                    bsp_lvgl_unlock();
                }
            }
        } else if (btn == BSP_BTN_DOWN) {
            if (s_curr_task.total_count > s_curr_task_offset + 1) {
                eevee_cmd_t cmd = {
                    .type = CMD_TASK_OFFSET_CHANGE,
                    .int_val = s_curr_task_offset + 1
                };
                xQueueSend(s_cmd_queue, &cmd, 0);
            } else {
                if (bsp_lvgl_lock(250)) {
                    eevee_badge_ui_set_page(EEVEE_PAGE_RECORDS);
                    bsp_lvgl_unlock();
                }
            }
        }
    } else if (page == EEVEE_PAGE_RECORDS) {
        if (btn == BSP_BTN_OK) {
            if (!s_wifi_connected) {
                if (bsp_lvgl_lock(250)) {
                    eevee_badge_ui_show_toast("离线无法录音", 0xEF4444);
                    bsp_lvgl_unlock();
                }
                return;
            }
            if (s_voice_task_handle != NULL) {
                ESP_LOGW(TAG, "Voice recording already active, ignoring");
                return;
            }
            ESP_LOGI(TAG, "Starting voice recording modal (click-to-toggle)...");
            s_voice_recording = true;
            s_voice_abort = false;
            memset(&s_curr_draft, 0, sizeof(s_curr_draft));
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_show_voice_view(true);
                eevee_badge_ui_update_voice_recording(0, 20);
                bsp_lvgl_unlock();
            }
            xTaskCreate(eevee_voice_task, "eevee_voice", 6144, NULL, 5, &s_voice_task_handle);
        } else if (btn == BSP_BTN_UP) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_TASKS);
                bsp_lvgl_unlock();
            }
        } else if (btn == BSP_BTN_DOWN) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_QR);
                bsp_lvgl_unlock();
            }
        }
    } else if (page == EEVEE_PAGE_QR) {
        if (btn == BSP_BTN_UP) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_RECORDS);
                bsp_lvgl_unlock();
            }
        } else if (btn == BSP_BTN_DOWN) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_SETTINGS);
                bsp_lvgl_unlock();
            }
        } else if (btn == BSP_BTN_OK) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_SETTINGS);
                bsp_lvgl_unlock();
            }
        }
    } else if (page == EEVEE_PAGE_SETTINGS) {
        if (btn == BSP_BTN_UP) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_QR);
                bsp_lvgl_unlock();
            }
        } else if (btn == BSP_BTN_DOWN) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_BADGE);
                bsp_lvgl_unlock();
            }
        } else if (btn == BSP_BTN_OK) {
            // 设置页面点击 OK：明确启动重新配网功能
            ESP_LOGI(TAG, "User clicked OK on Settings page to re-provision!");
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_show_toast("正在开启配网热点", 0x2563EB);
                bsp_lvgl_unlock();
            }
            eevee_app_enter_prov_mode();
        }
    }
}
