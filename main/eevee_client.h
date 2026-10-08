#pragma once

#include "eevee_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 拉取员工卡面个人资料 (GET /api/iot/me)
 */
bool eevee_client_fetch_me(const eevee_config_t *cfg, eevee_profile_t *out_profile);

/**
 * 翻页拉取待办审批工单 (GET /api/iot/tasks?offset=N)
 */
bool eevee_client_fetch_tasks(const eevee_config_t *cfg, int offset, eevee_task_t *out_task);

/**
 * 一键执行审批动作 (POST /api/iot/tasks/action)
 */
bool eevee_client_execute_task_action(const eevee_config_t *cfg,
                                      const char *app_id,
                                      const char *record_id,
                                      const char *action_id,
                                      const char *comment);

/**
 * 检查站内未读通知 (GET /api/iot/notifications/unread)
 */
bool eevee_client_fetch_notifications(const eevee_config_t *cfg,
                                      eevee_notification_t *out_notif);

/**
 * 标记通知已读 (POST /api/iot/notifications/read)
 */
bool eevee_client_mark_notifications_read(const eevee_config_t *cfg,
                                          const char *notif_id_or_all);

/**
 * 获取业务应用最新记录指标 (GET /api/iot/apps/:appId/records/latest)
 */
bool eevee_client_fetch_latest_record(const eevee_config_t *cfg,
                                      const char *app_id,
                                      eevee_record_t *out_record);

/**
 * 上报状态或数据打卡 (POST /api/iot/apps/:appId/records)
 */
bool eevee_client_report_record(const eevee_config_t *cfg,
                                const char *app_id,
                                float temp, float hum, float battery_volt);

/**
 * 录音进度回调 (返回当前已录制秒数)
 */
typedef void (*eevee_voice_progress_cb_t)(int elapsed_sec, void *user_data);

/**
 * 语音流式边录边传并获取解析草稿 (POST /api/v1/apps/:appId/record-drafts/parse)
 */
bool eevee_client_parse_voice_stream(const eevee_config_t *cfg,
                                     const char *app_id,
                                     volatile bool *p_recording,
                                     volatile bool *p_abort,
                                     eevee_voice_progress_cb_t progress_cb,
                                     void *user_data,
                                     eevee_draft_t *out_draft);

/**
 * 提交确认后的草稿业务数据创建记录 (POST /api/v1/apps/:appId/records)
 */
bool eevee_client_create_record(const eevee_config_t *cfg,
                                const char *app_id,
                                const char *values_json,
                                int *out_record_number);

#ifdef __cplusplus
}
#endif
