#include "config.h"

#include <Preferences.h>
#include <string.h>

namespace cyd {
namespace config {
namespace {

constexpr const char* kNamespace = "cyd";
constexpr const char* kKeySchema = "schema";
constexpr const char* kKeyHost   = "host";
constexpr const char* kKeyCred   = "cred";
constexpr const char* kKeyBlank  = "blank";

}  // namespace

bool load(DeviceConfig& out) {
  Preferences p;
  if (!p.begin(kNamespace, /*readOnly=*/true)) return false;

  const uint16_t schema = p.getUShort(kKeySchema, 0);
  if (schema == 0) { p.end(); return false; }              // unprovisioned

  // Migration is bounded to exactly one version back. Anything older is treated as unreadable and
  // the device returns to unprovisioned - never read as though its fields meant what they mean now.
  if (schema != kConfigSchemaVersion && schema != kConfigSchemaVersion - 1) {
    p.end();
    return false;
  }

  out.schemaVersion = kConfigSchemaVersion;
  p.getString(kKeyHost, out.pcHost, sizeof(out.pcHost));
  p.getString(kKeyCred, out.credential, sizeof(out.credential));

  // Migration in place. A version-1 record has no blanking key, so getUShort returns the default and
  // the device gains the feature switched on. That is the intended behaviour on update rather than an
  // accident of the default: defaulting to 0 would mean nobody gets CAP-BLANK without finding a
  // setting, and the capability is the reason the field exists.
  out.blankAfterMinutes = p.getUShort(kKeyBlank, kBlankMinutesDefault);
  p.end();

  // INV-BLANK-BOUND. An out-of-range stored value is not clamped - a record this firmware cannot
  // make sense of is treated as unreadable, which returns the device to unprovisioned rather than
  // guessing a period the operator never chose.
  if (out.blankAfterMinutes > kBlankMinutesMax) return false;

  return out.pcHost[0] != '\0';
}

bool save(const DeviceConfig& cfg) {
  Preferences p;
  if (!p.begin(kNamespace, /*readOnly=*/false)) return false;
  p.putUShort(kKeySchema, kConfigSchemaVersion);
  p.putString(kKeyHost, cfg.pcHost);
  p.putString(kKeyCred, cfg.credential);
  p.putUShort(kKeyBlank, cfg.blankAfterMinutes);
  p.end();
  return true;
}

void erase() {
  Preferences p;
  if (!p.begin(kNamespace, /*readOnly=*/false)) return;
  // Overwrite before removing. SEC-ERASE-OVERWRITE exists because a plain key deletion can leave
  // the old bytes sitting in flash - which is precisely the case this action is meant to serve.
  char blank[64];
  memset(blank, 'x', sizeof(blank) - 1);
  blank[sizeof(blank) - 1] = '\0';
  p.putString(kKeyHost, blank);
  p.putString(kKeyCred, blank);
  p.putUShort(kKeyBlank, 0);
  p.clear();
  p.end();
}

bool provisioned() {
  DeviceConfig cfg;
  return load(cfg);
}

}  // namespace config
}  // namespace cyd
