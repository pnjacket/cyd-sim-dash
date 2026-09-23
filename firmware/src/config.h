// Persisted device configuration.
//
// Realises ENTITY-DEVICECONFIG and part of COMPONENT-CONFIG. A singleton record in NVS, written
// whole. The schema carries its own version so a firmware update migrates rather than silently
// misreading an older record.
//
// Secrets stored here are unencrypted, which is a recorded decision rather than an oversight:
// SEC-STORAGE-PLAIN names the exposure — physical access plus a cable yields them — and judges it
// acceptable for a device mounted in your own house.

#ifndef CYD_CONFIG_H
#define CYD_CONFIG_H

#include <stddef.h>
#include <stdint.h>

namespace cyd {

constexpr uint16_t kConfigSchemaVersion = 1;

struct DeviceConfig {
  uint16_t schemaVersion = kConfigSchemaVersion;
  char     pcHost[64]    = {0};   // IP address or DNS name of the sim PC
  char     credential[64]= {0};   // gates the configuration page and OTA - SEC-CREDENTIAL-POLICY
};

namespace config {

// Reads the stored record. Returns false when unprovisioned, or when the record cannot be read -
// corrupt, or more than one schema version old. In every failing case the caller treats the device
// as unprovisioned and raises the portal, rather than trusting a partially-understood record.
bool load(DeviceConfig& out);

// Writes the record whole. Structural singleton: one namespace, one key set, one operation - the
// store cannot represent a second record.
bool save(const DeviceConfig& cfg);

// Overwrites stored values before deleting their keys. Plain NVS can otherwise leave the old bytes
// recoverable in flash, which would make the erase action a gesture rather than a mechanism.
void erase();

bool provisioned();

}  // namespace config
}  // namespace cyd

#endif  // CYD_CONFIG_H
