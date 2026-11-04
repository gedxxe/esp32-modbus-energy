// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace fort {
// Device-unique WPA2-PSK: 128 random bits rendered as 32 hex characters.
// Neither the password nor the random seed is compiled into a firmware image.
constexpr size_t AP_PASSWORD_BUFFER = 33;
constexpr size_t AP_RANDOM_BYTES = 16;
inline bool validAPPassword(const char *value) {
  if (!value || value[32] != '\0') return false;
  for (size_t i = 0; i < 32; ++i) {
    char c = value[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}
inline void encodeAPPassword(const uint8_t random[AP_RANDOM_BYTES], char out[AP_PASSWORD_BUFFER]) {
  static const char HEX[] = "0123456789abcdef";
  for (size_t i = 0; i < AP_RANDOM_BYTES; ++i) {
    out[2 * i] = HEX[random[i] >> 4];
    out[2 * i + 1] = HEX[random[i] & 15];
  }
  out[32] = '\0';
}
// A failed NVS write does not expose a shared fallback password. The valid,
// newly generated password stays in RAM for this boot and is shown only on OLED.
// Return true only when the credential has been read back from persistent store.
template <class Store, class RandomFill>
bool loadOrCreateAPPassword(Store &store, RandomFill randomFill, char out[AP_PASSWORD_BUFFER]) {
  memset(out, 0, AP_PASSWORD_BUFFER);
  if (store.getBytesLength("psk") == AP_PASSWORD_BUFFER &&
      store.getBytes("psk", out, AP_PASSWORD_BUFFER) == AP_PASSWORD_BUFFER &&
      validAPPassword(out)) return true;

  uint8_t random[AP_RANDOM_BYTES] = {};
  randomFill(random, sizeof(random));
  encodeAPPassword(random, out);
  memset(random, 0, sizeof(random));
  if (store.putBytes("psk", out, AP_PASSWORD_BUFFER) != AP_PASSWORD_BUFFER) return false;
  char readback[AP_PASSWORD_BUFFER] = {};
  const bool confirmed = store.getBytesLength("psk") == AP_PASSWORD_BUFFER &&
      store.getBytes("psk", readback, AP_PASSWORD_BUFFER) == AP_PASSWORD_BUFFER &&
      validAPPassword(readback) && memcmp(readback, out, AP_PASSWORD_BUFFER) == 0;
  memset(readback, 0, sizeof(readback));
  return confirmed;
}
} // namespace fort
