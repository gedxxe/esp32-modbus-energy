// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
namespace fort { namespace iot {
// ESP-IDF 5.5/Mbed TLS: SSL_ALLOC_FAILED is -0x7F00.
// TLS diagnostic getters may report its positive magnitude.
inline bool tlsAllocationFailure(int32_t error){
  return error==0x7F00 || error==-0x7F00;
}
} }
