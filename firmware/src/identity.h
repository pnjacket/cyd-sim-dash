// Device identity and firmware version.
//
// The identity derivation is a contract, not a convenience: the plugin and the firmware compute
// and compare it independently, so Domain & Data fixes it exactly — the six-byte station MAC, OUI
// byte first, each byte as two lowercase hex digits, no delimiter. Twelve characters matching
// ^[0-9a-f]{12}$. Derived, never stored.

#ifndef CYD_IDENTITY_H
#define CYD_IDENTITY_H

#include <stddef.h>

namespace cyd {

// Carried in every registration so a version mismatch stays diagnosable from the PC side, at the
// moment the panel itself is least able to help.
constexpr const char* kFirmwareVersion = "0.7.1";

// Writes the 12-character identity plus a terminator. `out` must hold at least 13 bytes.
// Returns out.
const char* deviceId(char* out, size_t len);

}  // namespace cyd

#endif  // CYD_IDENTITY_H
