#include "identity.h"

#include <esp_mac.h>
#include <stdio.h>

namespace cyd {

const char* deviceId(char* out, size_t len) {
  if (out == nullptr || len < 13) return out;
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(out, len, "%02x%02x%02x%02x%02x%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return out;
}

}  // namespace cyd
